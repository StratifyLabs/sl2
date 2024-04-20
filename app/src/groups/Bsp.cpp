// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <chrono.hpp>
#include <fs.hpp>
#include <printer.hpp>
#include <sos.hpp>
#include <sys.hpp>

#include <sos/Appfs.hpp>

#include "Bsp.hpp"

#include "utilities/Packager.hpp"

Bsp::Bsp() : Connector("os", "system") {}

var::StringViewList Bsp::get_command_list() const {
  StringViewList list = {
    "install",
    "invokebootloader",
    "reset",
    "publish",
    "configure",
    "ping",
    "authenticate",
    "pack"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool Bsp::execute_command_at(u32 list_offset, const Command &command) {

  switch (list_offset) {
  case command_install:
    return install(command);
  case command_invoke_bootloader:
    return invoke_bootloader(command);
  case command_reset:
    return reset(command);
  case command_publish:
    return publish(command);
  case command_configure:
    return configure(command);
  case command_ping:
    return ping(command);
  case command_authenticate:
    return authenticate(command);
  case command_pack:
    return pack(command);
  }

  return false;
}

bool Bsp::configure(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(configure, "configures the clock on a connected device")
      + GROUP_ARG_OPT(
        time,
        bool,
        false,
        "Synchronize the time of the device to the time of the host"));

  // connect by name
  // connect by serial number

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto is_time = command.get_argument_value("time");

  command.print_options(printer());

  if (!is_connection_ready()) {
    return printer().close_fail();
  }
  SlPrinter::Output printer_output_guard(printer());

  SL_PRINTER_TRACE("sync device time to host time");

  if (is_time == "true") {

    time_t host_time_t = time(nullptr);
    struct tm *host_time;

    host_time = localtime(&host_time_t);

    printer().key(
      "host.time",
      String().format(
        "%04d-%02d-%02d %02d:%02d:%02d",
        host_time->tm_year + 1900,
        host_time->tm_mon + 1,
        host_time->tm_mday,
        host_time->tm_hour,
        host_time->tm_min,
        host_time->tm_sec));

    connection()->set_time(host_time);
  }

  struct tm device_time {};
  connection()->get_time(&device_time);

  if (is_success()) {
    printer().key(
      "device.time",
      String().format(
        "%04d-%02d-%02d %02d:%02d:%02d",
        device_time.tm_year + 1900,
        device_time.tm_mon + 1,
        device_time.tm_mday,
        device_time.tm_hour,
        device_time.tm_min,
        device_time.tm_sec));
  }

  return is_success();
}

bool Bsp::install(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      install,
      "installs an OS package on a connected device that has been built on the "
      "host computer. If no *path* is "
      "specified, the workspace is searched for an OS package that matches the "
      "connected device.")
      + GROUP_ARG_OPT(
        build_target,
        string,
        release,
        "The build name which is usually 'release' or 'debug'.")
      + GROUP_ARG_OPT(
        delay,
        int,
        500,
        "The number of milliseconds to wait between reconnect retries (used "
        "with reconnect and retry).")
      + GROUP_ARG_OPT(
        hash,
        bool,
        false,
        "Install the image with a SHA256 hash appended to the end of the "
        "binary.")
      + GROUP_ARG_OPT(
        rekey,
        bool,
        false,
        "Overwrite any existing key that exists in the cloud.")
      + GROUP_ARG_OPT(
        synchronize_sync,
        bool,
        false,
        "Synchronize the installation with the cloud.")
      + GROUP_ARG_OPT(
        key,
        bool,
        false,
        "Install the image with a secret key inserted in the binary.")
      + GROUP_ARG_OPT(
        secretkey,
        string,
        <auto>,
        "The secret key to insert (use with `key`).")
      + GROUP_ARG_OPT(
        publickey_publicKey,
        string,
        <none>,
        "Public key to insert in the boot or OS image.")
      + GROUP_ARG_OPT(
        flashdevice_flashpath,
        string,
        <none>,
        "path to the flash device to use to install the OS")
      + GROUP_ARG_OPT(
        signkey_signKey,
        string,
        <none>,
        "Key id for signing the image.")
      + GROUP_ARG_OPT(
        signkeypassword_signKeyPassword,
        string,
        <null>,
        "Password to access private key used for signing the firmware.")
      + GROUP_ARG_OPT(
        path_p,
        string,
        <auto>,
        "The path to the OS project folder.")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <device>,
        "Write the image to this path rather than to the device.")
      + GROUP_ARG_OPT(
        architecture_arch,
        string,
        ignored,
        "This option is ignored (but is necessary to include for installing).")
      + GROUP_ARG_OPT(
        reconnect,
        bool,
        true,
        "Reconnect to the device after the OS package is installed.")
      + GROUP_ARG_OPT(
        retry,
        int,
        50,
        "The number of times to retry reconnecting after install (used with "
        "reconnect, and delay).")
      + GROUP_ARG_OPT(
        verify,
        bool,
        false,
        "Verify the OS image after it is installed."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto path = command.get_argument_value("path");
  const auto destination = command.get_argument_value("destination");
  const auto sign_key = command.get_argument_value("signkey");
  const auto public_key = command.get_argument_value("publickey");

  command.print_options(printer());

  if (!sign_key.is_empty() || !public_key.is_empty()) {
    if (!is_cloud_ready()) {
      return printer().close_fail();
    }
  }

  if (!workspace_settings().get_sign_key().is_empty()) {
    if (!is_cloud_ready()) {
      return printer().close_fail();
    }
  }

  if (path.is_empty() || destination.is_empty()) {
    if (!is_connection_ready(IsBootloaderOk(true))) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());

  PathString binary_path;
  PathString project_path;

  if (path.is_empty()) {
    // search current path for suitable projects
    SL_PRINTER_TRACE("search current directory for matching projects");
    auto project_list = FileSystem().read_directory(".");
    SL_PRINTER_TRACE(
      "directory has " | NumberString(project_list.count()) | " entries");
    IdString hardware_id
      = IdString().format("0x%08X", connection()->info().hardware_id());
    IdString hardware_id_alt
      = IdString().format("%08X", connection()->info().hardware_id());
    for (u32 i = 0; i < project_list.count(); i++) {
      if (FileSystem().get_info(project_list.at(i)).is_directory()) {

        api::ErrorGuard guard;
        Project project_settings = Project().import_file(File(
          FilePathSettings::project_settings_file_path(project_list.at(i))));

        auto type = project_settings.get_type();
        SL_PRINTER_TRACE(
          "checking " | project_settings.get_hardware_id() | " for match with "
          | hardware_id | " of type " | type);
        if (type != "<invalid>" && !type.is_empty()) {
          if (
          (type == "os")
          && ((project_settings.get_hardware_id() == hardware_id.string_view()) || (project_settings.get_hardware_id() == hardware_id_alt.string_view()))) {
            project_path = project_list.at(i);
            SL_PRINTER_TRACE("update path to " + project_path);
            break;
          }
        }
      }
    }
    if (project_path.is_empty()) {
      printer().troubleshoot(
        "If no `path` is specified, `sl` will search direct subfolders of the "
        "current directory for an OS project that "
        "has a hardware ID that matches the hardware ID of the connected "
        "device. In this case, not OS projects were "
        "found that matched.");
      APP_RETURN_ASSIGN_ERROR("no os package available in workspace");
    }
  } else {
    FileInfo source_directory_info = FileSystem().get_info(path);
    if (!source_directory_info.is_valid()) {
      printer().troubleshoot(
        "When specifying the path, the path is always relative to the current directory. The path can point to a "
        "directory that contains a valid `"
        + Project::file_name()
        + "` file that describes an application project or it can point to an application binary image file.");
      APP_RETURN_ASSIGN_ERROR(
        "the path specified does not refer to a valid directory or binary "
        "image file.");
    } else if (!source_directory_info.is_directory()) {
      binary_path = path;
      SL_PRINTER_TRACE("Using binary path " | binary_path);
    } else {
      project_path = path;
      SL_PRINTER_TRACE("Using project path " | project_path);
    }
  }

  printer().open_object(path.is_empty() ? project_path.string_view() : path);

  Installer installer(connection());

  if (!project_path.is_empty()) {
    SL_PRINTER_TRACE("importing project file");
    Project project = Project().import_file(
      File(FilePathSettings::project_settings_file_path(project_path)));
    if (!project.get_document_id().is_empty()) {
      session_settings().add_project_tag(project);
    }
    if (is_error()) {
      APP_RETURN_ASSIGN_ERROR("project not found at " | project_path);
    }
  }

  installer.install(get_installer_options(command)
                      .set_os(true)
                      .set_binary_path(binary_path)
                      .set_project_path(project_path));

  return is_success();
}

bool Bsp::invoke_bootloader(const Command &command) {

  printer().open_command("os.invokebootloader");
  add_session_report_tag("invokebootloader");

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      invokebootloader,
      "sends a software bootloader invocation request to the connected device.")
      + GROUP_ARG_OPT(
        delay,
        int,
        500,
        "The number of milliseconds to wait between reconnect attempts.")
      + GROUP_ARG_OPT(
        reconnect,
        bool,
        false,
        "Reconnect to the device after invoking the bootloader.")
      + GROUP_ARG_OPT(
        retry,
        int,
        5,
        "The number of times to retry connecting after the request."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto reconnect = command.get_argument_value("reconnect");
  const auto retry = command.get_argument_value("retry");
  const auto delay = command.get_argument_value("delay");

  command.print_options(printer());

  const chrono::MicroTime micro_time_delay
    = delay.to_integer() * 1_milliseconds;

  if (!is_connection_ready(IsBootloaderOk(true))) {
    return printer().close_fail();
  }

  if (connection()->is_bootloader()) {
    SL_PRINTER_TRACE("already connected to bootloader");
    return is_success();
  }

  SlPrinter::Output printer_output_guard(printer());

  connection()->reset_bootloader();

  if (reconnect == "true") {
    SL_PRINTER_TRACE("reconnecting");
    connection()->reconnect(retry.to_unsigned_long(), micro_time_delay);
  }

  return is_success();
}

bool Bsp::reset(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(reset, "resets the connected device.")
      + GROUP_ARG_OPT(
        bootloader_boot,
        bool,
        false,
        "Reset to bootloader (if available on the device).")
      + GROUP_ARG_OPT(
        delay,
        int,
        500,
        "The number of milliseconds to wait between reconnect "
        "attempts.os.reset:reconnect=true,delay=2000")
      + GROUP_ARG_OPT(
        reconnect,
        bool,
        false,
        "Reconnects to the device after the reset.")
      + GROUP_ARG_OPT(
        retry,
        int,
        5,
        "The number of times to retry connecting."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto reconnect = command.get_argument_value("reconnect");
  const auto retry = command.get_argument_value("retry");
  const auto bootloader = command.get_argument_value("bootloader");
  const auto delay = command.get_argument_value("delay");

  command.print_options(printer());

  chrono::MicroTime micro_time_delay = delay.to_integer() * 1_milliseconds;

  if (!is_connection_ready(IsBootloaderOk(true))) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  if (bootloader == "true") {
    connection()->reset_bootloader();
  } else {
    connection()->reset();
  }

  if (reconnect == "true") {
    SL_PRINTER_TRACE("reconnect to device");
    connection()->reconnect(retry.to_unsigned_long(), micro_time_delay);
  }

  if (is_success()) {
    printer().key("reset", String("complete"));
  }

  return is_success();
}

bool Bsp::publish(const Command &command) {

  // run with arguments
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      publish,
      "publishes an OS package to the Stratify cloud. The initial call to "
      "*os.publish* will acquire an ID that needs "
      "to be built into the binary. Subsequent calls to *os.publish* will "
      "publish the build.")
      + GROUP_ARG_OPT(changes, string, <none>, "deprecated")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "List what will be uploaded without uploading")
      + GROUP_ARG_OPT(
        signkey_signKey,
        string,
        <none>,
        "Key id for signing the image.")
      + GROUP_ARG_OPT(
        signkeypassword_signKeyPassword,
        string,
        <null>,
        "Password to access private key used for signing the firmware.")
      + GROUP_ARG_OPT(
        fork,
        bool,
        false,
        "Creates a new ID owned by the current user")
      + GROUP_ARG_OPT(
        header,
        bool,
        false,
        "publish the project settings to the `sl_config.h` header file.")
      + GROUP_ARG_OPT(
        path_p,
        string,
        <all>,
        "The path to the OS package to publish. All OS packages in the "
        "workspace are published if path isn't "
        "provided.")
      + GROUP_ARG_OPT(roll, bool, false, "Rolls the version number.")
      + GROUP_ARG_OPT(
        team,
        string,
        <public>,
        "Team for publishing the project. This is only needed on the initial "
        "publishing. If not present, the project "
        "settings will be checked for a team. If no team is specified or in "
        "the project settings, project will be "
        "published as a public project."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto path = command.get_argument_value("path");
  const auto header = command.get_argument_value("header");

  if (header == "true") {
    SlPrinter::Output po(printer());

    service::Project project = service::Project().import_file(
      File(FilePathSettings::project_settings_file_path(path)));
    if (is_error()) {
      printer().open_object(path);
      APP_RETURN_ASSIGN_ERROR(
        "failed to import "
        + FilePathSettings::project_settings_file_path(path));
    }

    printer().object(path, project.to_object());

    WorkspaceSettings::generate_configuration_header(path);
    if (is_error()) {
      APP_RETURN_ASSIGN_ERROR("failed to export `sl_config.h` for " + path);
    }
    return is_success();
  }

  if (!publish_project(command, Build::Type::os)) {
    return printer().close_fail();
  }

  return is_success();
}

bool Bsp::ping(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      ping,
      "collects and displays information from the connected device.")
      + GROUP_ARG_OPT(
        authentication_auth,
        bool,
        false,
        "load authentication info.")
      + GROUP_ARG_OPT(
        bootloader_boot,
        bool,
        false,
        "if target is not a bootloader, the command will fail."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto bootloader = command.get_argument_value("bootloader");
  const auto authentication = command.get_argument_value("authentication");

  if (!is_connection_ready(IsBootloaderOk(bootloader == "true"))) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  if (bootloader == "true") {

    if (!connection()->is_bootloader()) {
      APP_RETURN_ASSIGN_ERROR("device is not a bootloader");
    }

    SlPrinter::Object ping_object(
      printer().output(),
      connection()->info().path());
    printer()
      .output()
      .key_bool("bootloader", true)
      .object("connection", connection()->info());

    {
      bootloader_attr_t bootloader_attr;
      connection()->get_bootloader_attr(bootloader_attr);
      printer()
        .output()
        .open_object("booloaderAttributes")
        .key("version", NumberString(bootloader_attr.version, "0x%04X"))
        .close_object();
    }
    if (authentication == "true") {
      Printer::Object auth_object(printer().output(), "authentication");
      printer().key(
        "publicKey",
        View(connection()->get_public_key()).to_string<GeneralString>());
    }

  } else {

    SlPrinter::Object ping_object(
      printer().output(),
      connection()->info().path());

    printer()
      .output()
      .key_bool("bootloader", false)
      .object("connection", connection()->info());

    sos::Sys system("", connection()->driver());

    if (authentication == "true") {
      Printer::Object auth_object(printer().output(), "authentication");
      api::ErrorScope es;
      Auth auth("", connection()->driver());

      const auto public_key = auth.get_public_key();

      if (is_error()) {
        printer().info("authentication information not available");
      } else {
        printer().key("systemPublicKey", public_key.to_string());
      }

      const auto app_key_list
        = sos::Appfs(connection()->driver()).get_public_key_list();
      for (const auto &key : app_key_list) {
        Printer::Object auth_key_object(printer().output(), key.id());
        printer().key(
          "applicationPublicKey",
          key.get_key_view().to_string<GeneralString>());
      }
    }
  }

  return is_success();
}

bool Bsp::pack(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  // install the project -- app or BSP
  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(pack, "is used to pack up the current SDK for publishing.")
      + GROUP_ARG_REQ(
        name,
        string,
        <project name>,
        "name of the project for the SDK")
      + GROUP_ARG_REQ(
        path,
        string,
        <path to sdk>,
        "path to the SDK to process e.g. `SDK/local`")
      + GROUP_ARG_OPT(
        destination,
        string,
        <auto>,
        "path to the destination folder")
      + GROUP_ARG_OPT(
        sblob,
        bool,
        true,
        "create a secured blob rather than an executable")
      + GROUP_ARG_OPT(clean, bool, false, "delete the temporary folder")
      + GROUP_ARG_OPT(filter, string, <none>, "file pattern to filter")
      + GROUP_ARG_OPT(dryrun, bool, false, "don't upload just do a dry run"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto name = command.get_argument_value("name");
  const auto filter = command.get_argument_value("filter");
  const auto clean = command.get_argument_value("clean");
  const auto dry_run = command.get_argument_value("dryrun");
  const auto destination = command.get_argument_value("destination");
  const auto sblob = command.get_argument_value("sblob");
  const auto path = command.get_argument_value("path");

  command.print_options(printer());
  SlPrinter::Output printer_output_guard(printer());

  const auto effective_filter = [&]() {

    auto result = String(filter);
    if (result.is_empty()) {
      const auto default_filter_items = {
        "libc.a",
        "libg.a",
        "libgfortran.a",
        "libm.a",
        "librdpmon",
        ".py",
        "rdpmon",
        "crt",
        "redboot",
        "share/doc",
        "/bin/sl",
        ".spec",
        "linux",
        "rdimon",
        "_nano",
        "/cpu-init",
        "share/",
        "profile",
        "lib/libstdc",
        "lib/libsupc",
        "profile",
        "thumb/nofp",
        "thumb/v6-m",
        "thumb/v7-a",
        "thumb/v7-r",
        "thumb/v8"
      };
      for(const auto * entry: default_filter_items){
        result += entry;
        result += "?";
      }
      result.pop_back();
      if (sys::System::is_windows()) {
        result += "?/mingw";
      }
    }
    return result;
  }();

  const auto effective_destination = [&]() -> PathString {
    if (destination.is_empty()) {
      const auto workspace_destination
        = workspace_settings().get_sdk_directory();
      if (workspace_destination.is_empty()) {
        if (FileSystem().directory_exists("SDK")) {
          return "SDK";
        } else {
          return "";
        }
      }
      return workspace_destination;
    }
    return destination;
  }();

  if (effective_destination.is_empty()) {
    APP_RETURN_ASSIGN_ERROR(
      "No `destination` could be found. Please specify one");
  }

  Packager packager;

  printer().set_progress_key("securing");
  packager.set_keep_temporary(clean == "false")
    .publish(Packager::PublishOptions()
               .set_progress_callback(printer().progress_callback())
               .set_sblob(sblob == "true")
               .set_upload(false)
               .set_name("compiler")
               .set_dryrun(dry_run == "true")
               .set_remove_key(false)
               .set_archive_name("compiler")
               .set_filter(effective_filter)
               .set_source_path(path));

  if (!is_success()) {
    return printer().close_fail();
  }

  printer().key("source", packager.archive_path());

  const auto destination_file = [&]() {
    const PathString suffix
      = (sblob == "true")
          ? ("." & OperatingSystem::system_name() & "_sblob")
          : (
            OperatingSystem::get_executable_suffix().is_empty()
              ? PathString(".bin")
              : PathString(OperatingSystem::get_executable_suffix()));
    return effective_destination / name & suffix;
  }();

  printer()
    .key("destination", destination_file)
    .key("hash", View(packager.hash()).to_string<GeneralString>())
    .set_progress_key("copying");

  File(File::IsOverwrite::yes, destination_file)
    .write(
      File(packager.archive_path()),
      File::Write().set_progress_callback(printer().progress_callback()));

  return is_success();
}

bool Bsp::authenticate(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      authenticate,
      "authenticates the connection to the device so secure apps can be "
      "installed or the secret key can be "
      "retrieved.")
      + GROUP_ARG_OPT(
        key,
        string,
        <auto>,
        "The secret key to use for authentication. If no key is provided, the "
        "key will be fetched for the connected "
        "thing. You must have team permissions to access the thing."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView key = command.get_argument_value("key");
  command.print_options(printer());

  if (!is_connection_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(connection()->info().serial_number().to_string());

  if (key.is_empty()) {
    Thing thing(Sys::Info(connection()->info().sys_info()));
    key = thing.get_secret_key();
  }

  // key truncated to 128bits (32 bytes -> 64 characaters)
  constexpr u32 key_length = 256 * 2 / 8;

  if (key.length() < key_length) {
    printer().troubleshoot(
      "The key length is " + NumberString(key.length())
      + " but needs to be at least " + NumberString(key_length)
      + " in order to have 256 data bits");
    APP_RETURN_ASSIGN_ERROR("key is not valid.");
  }

  if (!Auth("", connection()->driver())
         .authenticate(var::Data::from_string(key))) {
    printer().key_bool("authenticated", false);
    APP_RETURN_ASSIGN_ERROR("failed to authenticate thing");
  }

  printer().key_bool("authenticated", true);
  return is_success();
}
