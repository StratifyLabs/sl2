// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include "Connection.hpp"
#include "Settings.hpp"

#if defined __win32
#include <initguid.h>
#include <ntddstor.h>
#include <setupapi.h>
#include <windows.h>
#endif

#include <hal.hpp>
#include <sos/link.h>
#include <sys.hpp>
#include <usb/usb_link_transport_driver.h>
#include <var.hpp>

#include "utilities/OperatingSystem.hpp"

Connection::Connection() : Group("connection", "conn") {}

int Connection::initialize() {
  APP_CALL_GRAPH_TRACE_FUNCTION();
  SL_PRINTER_TRACE_PERFORMANCE();
  SL_PRINTER_TRACE("Initialize connection");
  const SerialSettings &serial_options = workspace_settings().serial_settings();
  if (serial_options.is_valid()) {
    SL_PRINTER_TRACE("loading serial options");
    m_serial_options.baudrate = serial_options.get_baudrate();
    String parity = String(serial_options.get_parity()).to_lower();
    if (parity == "even") {
      m_serial_options.parity = 2;
    } else if (parity == "odd") {
      m_serial_options.parity = 1;
    } else {
      m_serial_options.parity = 0;
    }

    m_serial_options.stop_bits = serial_options.get_stop_bits();

    if (m_serial_options.stop_bits != 1 && m_serial_options.stop_bits != 2) {
      printer().warning(
        "invalid stop bit value `" + NumberString(m_serial_options.stop_bits)
        + "` (using 1)");
    }

    SL_PRINTER_TRACE(String().format(
      "serial options %dbps, %d stop bits, %d parity",
      m_serial_options.baudrate,
      m_serial_options.stop_bits,
      m_serial_options.parity));

    // make these settings effective by applying the options to the link driver
    set_driver_options(&m_serial_options);
  }
  return 0;
}

int Connection::finalize() { return 0; }

var::StringViewList Connection::get_command_list() const {
  APP_CALL_GRAPH_TRACE_FUNCTION();
  StringViewList list = {"list", "ping", "connect"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool Connection::execute_command_at(u32 list_offset, const Command &command) {
  APP_CALL_GRAPH_TRACE_FUNCTION();

  switch (list_offset) {
  case command_list:
    return list(command);
  case command_ping:
    return ping(command);
  case command_connect:
    return connect(command);
  }

  return false;
}

bool Connection::list(const Command &command) {
  APP_CALL_GRAPH_TRACE_FUNCTION();
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      list,
		"lists the serial/usb devices that are available as potential Stratify OS "
      "devices.")
      + GROUP_ARG_OPT(
        driver,
        string,
        <all>,
        "Driver to use such as a `serial` or `usb` driver."));

  if (!command.is_valid(reference, printer())) {
    if (command.get_argument_value("help") == "true") {
      return is_success();
    }
    return printer().close_fail();
  }

  const auto driver_name = command.get_argument_value("driver");

  command.print_options(printer());

  SlPrinter::Output printer_output_guard(printer());
  printer() << create_path_list(driver_name);
  return is_success();
}

bool Connection::ping(const Command &command) {
  APP_CALL_GRAPH_TRACE_FUNCTION();
  SL_PRINTER_TRACE_PERFORMANCE();
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      ping,
      "queries each serial port to see if a Stratify OS device is available. "
      "It can be used with 'blacklist' to "
      "prevent the workspace from trying to connect to unresponsive ports.")
      + GROUP_ARG_OPT(
        driver,
        string,
        <all>,
        "specifies either a `serial` or `usb` driver.")
      + GROUP_ARG_OPT(
        path_p,
        string,
        <all>,
        "pings the device on the specified path (such as COM4 or "
        "/dev/cu.usbmodem14333301)"));

  if (!command.is_valid(reference, printer())) {
    if (command.get_argument_value("help") == "true") {
      return is_success();
    }
    return printer().close_fail();
  }

  StringView path = command.get_argument_value("path");
  StringView driver_name = command.get_argument_value("driver");
  StringView blacklist = command.get_argument_value("blacklist");

  if (blacklist.is_empty()) {
    blacklist = "false";
  }

  command.print_options(printer());

  SlPrinter::Output printer_output_guard(printer());

  if (path.is_empty()) {
    const auto device_list = create_path_list(driver_name);
    if (device_list.count() == 0) {
      APP_RETURN_ASSIGN_ERROR("no devices available to ping");
    }

    SL_PRINTER_TRACE(
      "listing " | NumberString(device_list.count()) | " devices");
    for (const auto &device : device_list) {
      if (is_path_blacklisted(device) == false) {
        load_driver(device);

        if (Link::ping(device)) {
          connect(device);
          if (is_success()) {
            printer().object(device, info());
            disconnect();
          } else {
            printer().key(device, "no response");
          }

        } else {

          if (device.string_view().find("/usb") != StringView::npos) {
            printer().key(device, "no response");
          } else {

            SL_PRINTER_TRACE("No Ping Received");
            SL_PRINTER_TRACE(
              String().format("is blacklist? %d", blacklist == "true"));
            if (blacklist == "true") {
              SL_PRINTER_TRACE(
              "adding `" + device + "` to blacklist, use `sl conn.ping:blacklist=false` to clear all");

              // StringViewList list = workspace_settings().blacklist();
              // list.push_back(devices.at(i));
              workspace_settings().set_blacklist(
                workspace_settings().get_blacklist().push_back(device));
              // workspace_settings().blacklist().list().push_back(devices.at(i));
              workspace_settings().save();
            }
          }
        }
      } else {
        SL_PRINTER_TRACE("skipping blacklisted port " + device);
      }
    }

  } else {

    SlPrinter::Object path_object(printer().output(), path);
    load_driver(path);
    if (Link::ping(path)) {
      connect(path);
      if (is_success()) {
        printer().object(path, info());
        disconnect();
      } else {
        APP_RETURN_ASSIGN_ERROR("failed to connect to device");
      }
    } else {
      APP_RETURN_ASSIGN_ERROR("failed to ping device");
    }
  }

  return is_success();
}

bool Connection::connect(const Command &command) {
  APP_CALL_GRAPH_TRACE_FUNCTION();
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    "connect:description=opt_string|The `connection.connect` command will "
    "connect to a specific device. The connection "
    "will be made automatically if this command is not used but if there is "
    "more than one device attached, the "
    "automatic connection will make an arbitrary choice on which device to "
    "connect to.||connection.connect|"
#if !defined __win32
    ",path_p=opt_string_<auto>|connects to the specified device using the host "
    "device file "
    "path.||conn.connect:path=@serial/dev/tty.usbmodem123837|"
#else
    ",path_p=opt_string_<auto>|connects to the specified device using the host "
    "device file "
    "path.||conn.connect:path=@serial/COM1|"
#endif
      + GROUP_ARG_OPT(
        baudrate_br,
        int,
        <460800>,
        "baudrate to use for serial connections.")
      + GROUP_ARG_OPT(
        stopbits_sb,
        int,
        <1>,
        "stop bits to use for serial connections as 1 or 2 (only applicable if "
        "UART is used)")
      + GROUP_ARG_OPT(
        parity,
        string,
        <none>,
        "use for serial connections as 'odd' or 'even' or 'none' (only "
        "applicable if UART is used)")
      + GROUP_ARG_OPT(
        save,
        bool,
        false,
        "saves the serial settings as the defaults for the workspace (save "
        "with no serial options to restore defaults).")
      + GROUP_ARG_OPT(legacy, bool, false, "use the legacy protocol.")
      + GROUP_ARG_OPT(retry_r, int, 5, "number of times to retry connecting.") +
      //",retry_r=opt_int_5|number of times to retry
      // connecting.||conn.connect:retry=5|"
      GROUP_ARG_OPT(
        delay_d,
        int,
        500,
        "number of milliseconds to wait between reconnect attempts.")
    //",delay_d=opt_int_500|number of milliseconds to wait between reconnect
    // attempts.||conn.connect:retry=5,delay=1000|"
  );
  // connect by name
  // connect by serial number

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  StringView path = command.get_argument_value("path");
  StringView retry = command.get_argument_value("retry");
  StringView legacy = command.get_argument_value("legacy");
  StringView delay = command.get_argument_value("delay");

  bool is_override_serial_options = false;
  StringView baudrate = command.get_argument_value("baudrate");
  StringView stop_bits = command.get_argument_value("stopbits");
  StringView parity = command.get_argument_value("parity");
  StringView is_save = command.get_argument_value("save");

  u32 retry_count = static_cast<u32>(retry.to_unsigned_long());
  u32 delay_ms = static_cast<u32>(delay.to_integer());

  if (parity.is_empty()) {
    parity = "none";
  } else {
    is_override_serial_options = true;
  }
  if (stop_bits.is_empty()) {
    stop_bits = "1";
  } else {
    is_override_serial_options = true;
  }
  if (baudrate.is_empty()) {
    baudrate = "460800";
  } else {
    is_override_serial_options = true;
  }

  // serial_settings refers to original object
  SerialSettings serial_settings = workspace_settings().serial_settings();
  if (is_override_serial_options) {
    SL_PRINTER_TRACE("overriding default serial options");
    m_serial_options.baudrate = baudrate.to_integer();
    m_serial_options.stop_bits = stop_bits.to_integer();
    if (parity == "odd") {
      m_serial_options.parity = 1;
    } else if (parity == "even") {
      m_serial_options.parity = 2;
    } else {
      m_serial_options.parity = 0;
    }
    set_driver_options(&m_serial_options);

    if (is_save == "true") {
      serial_settings.set_baudrate(baudrate.to_integer())
        .set_stop_bits(stop_bits.to_integer())
        .set_parity(parity);
      workspace_settings().save();
    }

  } else if (is_save == "true") {
    serial_settings.set_baudrate(0).set_stop_bits(0).set_parity("");
    workspace_settings().save();
  }

  if (serial_settings.is_valid() && !is_override_serial_options) {
    SL_PRINTER_TRACE("loading serial settings from workspace");
    baudrate = NumberString(serial_settings.get_baudrate());
    stop_bits = NumberString(serial_settings.get_stop_bits());
    parity = serial_settings.get_parity().to_string();
  }

  command.print_options(printer());

  SlPrinter::Output printer_output_guard(printer());
  Link::DriverPath link_driver_path(path);

  if (link_driver_path.is_valid() == false) {
    APP_RETURN_ASSIGN_ERROR(
      link_driver_path.path().string_view() + " is not a valid device path");
  }

  if (link_driver_path.is_partial()) {

    SL_PRINTER_TRACE("try to connect with partial path " | path);
    try_connect(TryConnect()
                  .set_driver_path(link_driver_path)
                  .set_retry_count(retry_count)
                  .set_delay_interval(delay_ms * 1_milliseconds));

  } else {
    SL_PRINTER_TRACE("Connect with full path" | path);
    load_driver(path);
    SL_PRINTER_TRACE("Connect to " | path);
    connect(
      path,
      legacy == "true" ? Link::IsLegacy::yes : Link::IsLegacy::no);

    if (is_error()) {
      APP_RETURN_ASSIGN_ERROR("failed to connect to " + path);
    }
  }

  if (is_connected()) {
    printer().object(path.is_empty() ? "<any>" : path, info());
  } else {
    APP_RETURN_ASSIGN_ERROR("failed to connect");
  }

  return is_success();
}

bool Connection::try_connect(const TryConnect &options) {
  StringView lookup_device_path;


  TryConnect options_with_session_path = options;

  SL_PRINTER_TRACE("checking link driver path " + options.driver_path().path());

  if (
    options.driver_path().path().is_empty()
    && !session_settings().path().is_empty()) {
    options_with_session_path = TryConnect().set_driver_path(
      Link::DriverPath(session_settings().path()));
  }


  SL_PRINTER_TRACE("Using driver " | Link::DriverPath(options_with_session_path.driver_path().path()).get_driver_name());

  printer().output().set_progress_key("attempts");
  try_connect_to_any(options_with_session_path);
  printer().output().set_progress_key("progress");

  if (is_success() && is_connected()) {
    session_settings().set_path(options.driver_path().path());
    session_settings().set_serial_number(info().serial_number().to_string());
    return true;
  }

  return false;
}

void Connection::try_connect_to_any(const TryConnect &options) {
  u32 retry_counter = 0;


  API_RETURN_IF_ERROR();
  SL_PRINTER_TRACE("try to connect to any device");
  SL_PRINTER_TRACE("preferred driver path " | options.driver_path().path());

  printer().update_progress(
    static_cast<int>(++retry_counter),
    api::ProgressCallback::indeterminate_progress_total());

  do {
    API_RESET_ERROR();

    const auto path_list
      = create_path_list(options.driver_path().get_driver_name());

    for (u32 i = 0; i < path_list.count() && is_connected() == false; i++) {
      const auto path = path_list.at(i);

      SL_PRINTER_TRACE("given path " | options.driver_path().path());
      SL_PRINTER_TRACE("compare to path " | path);

      if (options.driver_path() == Link::DriverPath(path)) {
        if (is_path_blacklisted(path) == false) {
          API_RESET_ERROR();

          SL_PRINTER_TRACE(String().format(
            "attemping to connect to '%s'; attempt %d of %d",
            path.cstring(),
            retry_counter + 1,
            options.retry_count()));
          load_driver(path);
          SL_PRINTER_TRACE("Connect to " | path);
          connect(path);

        } else {
          SL_PRINTER_TRACE(
            String().format("%s is blacklisted", path.cstring()));
        }
      } else {
        SL_PRINTER_TRACE("Skipping " | path);
      }
    }

    if (!is_connected() && (retry_counter < options.retry_count()+1)) {
      API_RESET_ERROR();
      SL_PRINTER_TRACE(String().format(
        "no devices available yet; attempt %d of %d",
        retry_counter + 1,
        options.retry_count()));

      SL_PRINTER_TRACE(String().format(
        "waiting %d milliseconds",
        options.delay_interval().milliseconds()));

      chrono::wait(options.delay_interval());
      retry_counter++;

      printer().update_progress(
        static_cast<int>(retry_counter),
        api::ProgressCallback::indeterminate_progress_total());
    }

  } while ((is_connected() == false)
           && (retry_counter < (options.retry_count() + 1)));

  printer().update_progress(0, 0);
}

bool Connection::is_path_blacklisted(const var::StringView path) {
  const StringViewList blacklist = workspace_settings().get_blacklist();
  if (blacklist.find_offset(path) < blacklist.count()) {
    return true;
  }
  return false;
}

fs::PathList Connection::create_path_list(const var::StringView driver_name) {
  if (sys::System::is_windows()) {
    return create_path_list_windows(driver_name);
  }

  return create_path_list_posix(driver_name);
}

PathList Connection::create_path_list_serial() {
  PathList result;
  link_load_default_driver(driver());
  auto serial_list = get_path_list();
  for (const auto &path : serial_list) {
    if (path.string_view().find("/serial") == 0) {
      result.push_back(path);
    } else {
      result.push_back(PathString("/serial") & path);
    }
    SL_PRINTER_TRACE("adding serial device " + path);
  }
  return result;
}

fs::PathList Connection::create_path_list_usb() {
  usb_link_transport_load_driver(driver());
  SL_PRINTER_TRACE("load USB driver");
  const auto usb_list = get_path_list();
  SL_PRINTER_TRACE("found " + NumberString(usb_list.count()) + " devices");
  return usb_list;
}

fs::PathList
Connection::create_path_list_windows(const var::StringView driver_name) {
  fs::PathList result;
  if (driver_name.is_empty() || (driver_name == "serial")) {
    SL_PRINTER_TRACE("Looking for serial devices");
    link_load_default_driver(driver());
      var::Vector<Link::DriverPath> serial_list
        = OperatingSystem::get_stratify_os_serial_devices_port_list();
      for (const Link::DriverPath &path : serial_list) {
        result.push_back(path.path());
      }

  }

  if (driver_name.is_empty() || (driver_name == "usb")) {
    result << create_path_list_usb();
  }

  return result;
}

fs::PathList
Connection::create_path_list_posix(const var::StringView driver_name) {
  fs::PathList result;

  if (driver_name.is_empty() || (driver_name == "usb")) {
    result = create_path_list_usb();
  }

  if (driver_name.is_empty() || (driver_name == "serial")) {
    result << create_path_list_serial();
  }

  return result;
}

#define BUFF_LEN 20

var::String Connection::get_serial_port_path_from_hardware_id(
  const sos::Link::DriverPath &driver_path) {
#if defined __win32
  HDEVINFO device_info_set;
  DWORD device_index = 0;
  SP_DEVINFO_DATA device_info_data;
  PCSTR DevEnum = "USB";
  BYTE szBuffer[1024] = {0};
  DWORD ulPropertyType;
  DWORD dwSize = 0;
  DWORD Error = 0;

  device_info_set = SetupDiGetClassDevs(
    nullptr,
    DevEnum,
    nullptr,
    DIGCF_ALLCLASSES | DIGCF_PRESENT);

  if (device_info_set == INVALID_HANDLE_VALUE) {
    return String();
  }

  String lookup_id = "USB\\VID_" + driver_path.get_vendor_id() + "&PID_"
                     + driver_path.get_product_id();

  if (driver_path.get_interface_number().is_empty() == false) {
    lookup_id += "&MI_"
                 + NumberString(
                   driver_path.get_interface_number().to_integer(),
                   "%02d");
  }

  View(device_info_data).fill<u8>(0);

  SL_PRINTER_TRACE(
    String().format("lookup serial port for %s", lookup_id.cstring()));
  device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  // Receive information about an enumerated device
  while (
    SetupDiEnumDeviceInfo(device_info_set, device_index, &device_info_data)) {
    device_index++;

    // Retrieves a specified Plug and Play device property
    if (SetupDiGetDeviceRegistryProperty(
          device_info_set,
          &device_info_data,
          SPDRP_HARDWAREID,
          &ulPropertyType,
          static_cast<BYTE *>(szBuffer),
          sizeof(szBuffer), // The size, in bytes
          &dwSize)) {
      HKEY hDeviceRegistryKey;
      // Get the key
      hDeviceRegistryKey = SetupDiOpenDevRegKey(
        device_info_set,
        &device_info_data,
        DICS_FLAG_GLOBAL,
        0,
        DIREG_DEV,
        KEY_READ);

      if (hDeviceRegistryKey == INVALID_HANDLE_VALUE) {
        Error = GetLastError();

        break; // Not able to open registry
      } else {
        // Read in the name of the port
        BYTE pszPortName[BUFF_LEN];
        DWORD dwSize = sizeof(pszPortName);
        DWORD dwType = 0;

        View(pszPortName).fill<u8>(0);

        if (
          (RegQueryValueEx(
             hDeviceRegistryKey,
             "PortName",
             nullptr,
             &dwType,
             static_cast<LPBYTE>(pszPortName),
             &dwSize)
           == ERROR_SUCCESS)
          && (dwType == REG_SZ)) {

          SL_PRINTER_TRACE(
            String("port is ") + var::String((char *)pszPortName));
          SL_PRINTER_TRACE(String("sz is ") + var::String((char *)szBuffer));

          SL_PRINTER_TRACE(String().format(
            "compare '%s' == '%s'",
            lookup_id.cstring(),
            szBuffer));
          if (lookup_id == var::String(reinterpret_cast<char *>(szBuffer))) {
            return String(var::String(reinterpret_cast<char *>(pszPortName)));
          }

#if 0
					// Check if it really is a com :port
					if( _tcsnicmp(
								pszPortName,
								_T("COM"),
								3
								) == 0)
					{

						int nPortNr = _ttoi( pszPortName + 3 );
						if( nPortNr != 0 )
						{
							_tcscpy_s(pszComePort,BUFF_LEN,pszPortName);

						}

					}
#endif
        }

        // Close the key now that we are finished with it
        RegCloseKey(hDeviceRegistryKey);
      }
    }
  }

  if (device_info_set) {
    SetupDiDestroyDeviceInfoList(device_info_set);
  }
#else

#endif
  return String();
}
