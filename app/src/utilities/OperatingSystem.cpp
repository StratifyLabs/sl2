
#if defined __win32

#include <winsock2.h>

#include <windows.h>

#include <initguid.h>
#include <ntddstor.h>
#include <setupapi.h>

#else
#include <sys/stat.h>
#endif

#include <sos.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "../SlPrinter.hpp"
#include "App.hpp"
#include "OperatingSystem.hpp"

namespace {
var::PathString path_to_sl;
}

var::Vector<sos::Link::DriverPath>
OperatingSystem::get_stratify_os_serial_devices_port_list() {
  var::Vector<Link::DriverPath> result;
  const auto driver_path_list = get_device_list();
  for (const Link::DriverPath &path : driver_path_list) {
    if (
      (path.get_vendor_id().to_string().to_upper() == "20A0")
      && (path.get_product_id().to_string().to_upper() == "41D5")) {

      const auto link_driver_path
        = Link::DriverPath(Link::DriverPath::Construct()
                             .set_type(Link::Type::serial)
                             .set_device_path(path.get_serial_number()));
      result.push_back(link_driver_path);
    }
  }

  return result;
}

#define BUFF_LEN 20

var::Vector<sos::Link::DriverPath> OperatingSystem::get_device_list() {
  var::Vector<Link::DriverPath> result;
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
    printf("no results\n");
    return result;
  }

#if 0
	String lookup_id;
	String device_id =
			"USB\\VID_" +
			link_driver_path.vendor_id() +
			"&PID_" +
			link_driver_path.product_id() +
			"&MI_" +
			link_driver_path.interface_number();

	device_id.to_upper();

	lookup_id << device_id;
#endif

  View(device_info_data).fill<u8>(0);

  device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  // Receive information about an enumerated device
  while (
    // printf("look for a device\n");
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

          const String windows_usb_path
            = var::String(reinterpret_cast<char *>(szBuffer));
          const String windows_com_port
            = var::String(reinterpret_cast<char *>(pszPortName));

          JsonObject object;
          auto path_component_list = windows_usb_path.split("\\&");
          for (const StringView &component : path_component_list) {
            auto items = component.split("_");
            if (items.count() == 2) {
              object.insert(items.at(0), JsonString(items.at(1)));
            }
          }

          result.push_back(Link::DriverPath(
            Link::DriverPath::Construct()
              .set_type(Link::Type::usb)
              .set_vendor_id(object.at("VID").to_string())
              .set_product_id(object.at("PID").to_string())
              .set_interface_number(object.at("MI").to_string())
              .set_serial_number(windows_com_port)));
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
  return result;
}

bool OperatingSystem::archive(const ArchiveOptions &options) {
  String system_command;
  String executable = "7z" + get_executable_suffix();

  // use 7z to compress the directory (windows)
#if 0
  system_command = executable
                   + " a -t7z -m0=BCJ2 -m1=LZMA2:d=1024m -aoa -r -sfx \""
                   + options.destination_file_path() + "\"";
#endif

  const auto arguments = Process::Arguments(Process::which(executable))
                           .push("a")
                           .push("-t7z")
                           .push("-m0=BCJ2")
                           .push("-m1=LZMA2:d=1024m")
                           .push("-aoa")
                           .push("-r")
                           .push("-sfx")
                           .push(options.destination_file_path());

  if (
    AppAccess::execute_system_command(
      arguments,
      options.source_directory_path())
    < 0) {
    return false;
  }

  return true;
}

bool OperatingSystem::change_mode(const var::StringView file_path, u32 mode) {
  if (FileSystem().exists(file_path) == false) {
    return false;
  }

#if !defined __win32
  if (chmod(PathString(file_path).cstring(), mode) < 0) {
    return false;
  }

#endif
  return true;
}

bool OperatingSystem::extract(const ExtractOptions &options) {

  if (!FileSystem().directory_exists(options.destination_directory_path())) {
    FileSystem().create_directory(options.destination_directory_path());
  }

  if (change_mode(options.source_file_path(), 0755) == false) {
    return false;
  }

#if 0
  const String path
    = String(options.source_file_path())
        .replace(String::Replace().set_old_string(" ").set_new_string("\\ "));
#endif

  const auto arguments
    = Process::Arguments(options.source_file_path()).push("-y");
  if (
    AppAccess::execute_system_command(
      arguments,
      options.destination_directory_path())
    < 0) {
    return false;
  }

  return true;
}

PathString OperatingSystem::search_path(const var::StringView application) {
  char *path = getenv("PATH");
  if (path == nullptr) {
    return PathString();
  }

  API_RETURN_VALUE_IF_ERROR(PathString());
  const StringView separator = sys::System::is_windows() ? ";" : ":";

  auto path_list = StringView(path).split(separator);

  for (const auto path_item : path_list) {
    auto list = FileSystem().read_directory(path_item);
    for (const auto &list_item : list) {
      if (list_item == application) {
        return PathString(path_item);
      }
    }
    api::ExecutionContext::reset_error();
  }

  return PathString();
}

var::String OperatingSystem::download_file(
  const inet::Url url,
  const sos::Link::File &destination_file,
  const api::ProgressCallback *progress_callback) {
  inet::Http::Status status;

  // need to determine secure or not secure
  if (url.protocol() == inet::Url::Protocol::https) {
    status = inet::HttpSecureClient()
               .connect(url.domain_name())
               .get(
                 url.path(),
                 inet::HttpClient::Get()
                   .set_response(&destination_file)
                   .set_progress_callback(progress_callback))
               .response()
               .status();
  } else {
    status = inet::HttpClient()
               .connect(url.domain_name())
               .get(
                 url.path(),
                 inet::HttpClient::Get()
                   .set_response(&destination_file)
                   .set_progress_callback(progress_callback))
               .response()
               .status();
  }

  if (status == inet::Http::Status::ok) {
    return String("");
  }

  return String(
    "Failed to download file with http status "
    + inet::Http::to_string(status));
}

var::PathString OperatingSystem::get_path_to_sl() {
  API_ASSERT(!path_to_sl.is_empty());
  return path_to_sl;
}

void OperatingSystem::resolve_path_to_sl(var::StringView invoked_sl_path) {
  if( invoked_sl_path.starts_with("sl") ){
    path_to_sl = Process::which("sl");
    return;
  }

  path_to_sl = invoked_sl_path;
  return;
}
