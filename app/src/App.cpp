#include <chrono.hpp>
#include <fs.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "App.hpp"
#include "settings/PackageSettings.hpp"

#if defined SL_CLOUD_API_KEY
#define API_KEY SL_CLOUD_API_KEY
#else
#define API_KEY ""
#endif

bool AppAccess::is_cloud_service_available(){
  #if defined SL_CLOUD_API_KEY
    return true;
  #else
    return false;
  #endif
}

AppMembers::AppMembers()
  : cloud_service(API_KEY, "stratifylabs") {}

int App::initialize(const Cli &cli) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("App");
  CloudObject::set_default_printer(printer().output());
  CloudAccess::set_default_cloud_service(cloud_service());

  if (cli.get_option("verbose") == "trace") {
    // if tracing, start now
    printer().set_verbose_level("trace");
  }

  OperatingSystem::resolve_path_to_sl(cli.get_path());

  SL_PRINTER_TRACE("sl Path: " + OperatingSystem::get_path_to_sl());

  if (session_settings().is_sdk_invoke_mismatch()) {
    SL_PRINTER_TRACE("SOS_SDK_ENV mismatched to invoked sl");
  }

  SL_PRINTER_TRACE_PERFORMANCE();
  SL_PRINTER_TRACE("load credentials");
  load_credentials();

  SL_PRINTER_TRACE("load global settings");
  global_settings().load();

  SL_PRINTER_TRACE("set workspace verbose level");
  printer().set_verbose_level(workspace_settings().get_verbose());

  SL_PRINTER_TRACE("check environment");
  StringView suffix = OperatingSystem::get_executable_suffix();
  if (sys::System::is_windows()) {
    PathString result = OperatingSystem::search_path("sh.exe");
    session_settings().set_msys(result.is_empty() == false);
    if (session_settings().is_msys()) {
      printer().set_bash();
    }
  } else {
    printer().set_bash();
  }

  SL_PRINTER_TRACE("sl invoked as " + cli.get_name());

  SL_PRINTER_TRACE(
    "is slu.exe ? "
    + ((cli.get_name().find(("slu.exe")) != String::npos) ? String("true") : String("false")));
  SL_PRINTER_TRACE(
    "is sla.exe ? "
    + ((cli.get_name().find(("sla.exe")) != String::npos) ? String("true") : String("false")));
  SL_PRINTER_TRACE(
    "is slu ? "
    + ((fs::Path::name(cli.get_name()) == sl_update_name()) ? String("true") : String("false")));
  SL_PRINTER_TRACE(
    "is sla ? "
    + ((fs::Path::name(cli.get_name()) == sl_admin_name()) ? String("true") : String("false")));

  if (
    cli.get_name().find(("slu.exe")) != String::npos
    || fs::Path::name(cli.get_name()) == sl_update_name()) {
    SL_PRINTER_TRACE("check updates");
    printer().open_command("update");

    const PathString source
      = workspace_settings().sl_location() / sl_update_name() & suffix;

    const PathString destination
      = workspace_settings().sl_location() / "sl" & suffix;

    printer().key("version", StringView(VERSION));
    printer().info("updating sl");

    if (File(
          File::IsOverwrite::yes,
          destination,
          fs::OpenMode::write_only(),
          fs::Permissions(0755))
          .write(File(source.string_view()))
          .is_error()) {
      printer()
        .error("failed to update sl")
        .warning("could not copy " + source + " to " + destination)
        .close_fail();
    } else {
      printer().close_success();
    }
    exit(0);
  }

  SL_PRINTER_TRACE("App initialized");
  return 0;
}

bool App::create_sl_copy(const StringView destination) {

  SL_PRINTER_TRACE("check updates");
  printer().open_command("update");
  StringView suffix = OperatingSystem::get_executable_suffix();

  const PathString source = workspace_settings().sl_location() / "sl" & suffix;

  const bool is_dest_folder
    = FileSystem().exists(destination)
      && FileSystem().get_info(destination).is_directory();

  const PathString slc_dest
    = destination.is_empty()
        ? workspace_settings().sl_location() / sl_copy_name() & suffix
      : is_dest_folder ? destination / "sl" & suffix
                       : PathString(destination);

  if (File(
        File::IsOverwrite::yes,
        slc_dest,
        OpenMode::read_write(),
        Permissions(0755))
        .write(File(source.string_view()))
        .is_error()) {
    printer().error("failed to create a copy of `sl`");
    return false;
  }

  return true;
}

void App::launch_browser_login() {
  const String url = String("https://app.stratifylabs.co/#/sl?os=")
                     + sys::System::operating_system_name();
  // mac

  const String command
    = (System::is_macosx() ? String("open") : String("start")) + " " + url;

  if (system(command.cstring()) != 0) {
    SL_PRINTER_TRACE("launch browser error");
  }
}

int App::initialize_cloud(IsForceDownloadSettings) {
  API_RETURN_VALUE_IF_ERROR(-1);
  SL_PRINTER_TRACE_PERFORMANCE();

  if (is_error()) {
    API_RESET_ERROR();
  }

  if (
    cloud_service().cloud().credentials().get_token_timestamp().age()
    > 59_minutes) {
    SL_PRINTER_TRACE("refresh cloud credentials");
    SL_PRINTER_TRACE(
      "refresh using uid " + cloud_service().cloud().credentials().get_uid());
    if (cloud_service().cloud().refresh_login().is_error()) {
      SL_PRINTER_TRACE(
        "login traffic " + cloud_service().cloud().traffic() + "\n");
      printer().troubleshoot(
        "Please execute `sl cloud.login` (this will launch the browser) then "
        "copy and paste your login code from the web application.");
      return -1;
    } else {
      credentials()
        = Credentials(cloud_service().cloud().credentials().to_object())
            .set_global(credentials().is_global());

      save_credentials();
    }
  }

  return 0;
}

int App::finalize() {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("App");
  int result = 0;

#if 0
	result = check_for_update();
#endif

  global_settings().save();
  workspace_settings().save();
  return result;
}

void App::load_credentials() {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("App");
  API_RETURN_IF_ERROR();

  credentials().load();
  cloud_service().cloud().credentials().copy(credentials());

  // disregard the error if the credentials failed to load
  if (is_error()) {
    API_RESET_ERROR();
  }

  SL_PRINTER_TRACE("uid " + cloud_service().cloud().credentials().get_uid());
  SL_PRINTER_TRACE(
    "token " + cloud_service().cloud().credentials().get_token());
  SL_PRINTER_TRACE(
    "refresh token "
    + cloud_service().cloud().credentials().get_refresh_token());
  SL_PRINTER_TRACE(
    "age "
    + NumberString(cloud_service()
                     .cloud()
                     .credentials()
                     .get_token_timestamp()
                     .age()
                     .ctime()));
}

int App::check_for_update(IsPreview is_preview) {
  APP_CALL_GRAPH_TRACE_FUNCTION();

  SL_PRINTER_TRACE("time to check for update?");
  if (!session_settings().is_update_checked() && !printer().is_vanilla()) {

    SL_PRINTER_TRACE("checking for update");
    session_settings().set_update_checked();

    PackageDescription package_description
      = PackageSettings::get_cloud_package_description(
        PackageSettings::PackageOptions().set_name("sl").set_preview(
          is_preview == IsPreview::yes));

    const auto current_version = sys::Version(VERSION);
    const auto latest_version = sys::Version(package_description.get_version());

    SL_PRINTER_TRACE(
      "latest version : " + latest_version.string_view()
      + " > current version: " + current_version.string_view());

    if (latest_version > current_version) {
      if (
        session_settings().is_request_update()
        || (global_settings().get_update_version() != latest_version.string_view())) {

        printer().open_command("update");
        SlPrinter::Output printer_output_guard(printer());
        printer().open_object("check");
        printer().key("system", OperatingSystem::system_name());
        printer().key("path", OperatingSystem::get_path_to_sl());
        printer().key("currentVersion", current_version.string_view());
        printer().key("latestVersion", latest_version.string_view());
        if (
          download_sl_image(latest_version.string_view(), is_preview)
          == false) {
          printer().error(
            "failed to download sl version " + latest_version.string_view());
        }

        printer().key(
          "update",
          String("use `slu` to apply the updated version of sl"));

        SL_PRINTER_TRACE(
          "saving latest downloaded version in global settings "
          + latest_version.string_view());
        global_settings().set_update_version(latest_version.string_view());

        printer().close_success();
        return 1;
      }
    }
  }
  return 0;
}

bool App::download_sl_image(
  const var::StringView version,
  IsPreview is_preview) {
  String binary_directory;
  String path;
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("App");

  binary_directory = workspace_settings().sl_location();
  if (binary_directory.is_empty()) {
    SL_PRINTER_TRACE("failed to find default install directory");
    return false;
  }

  String sl_name;
  const PackageDescription package_description
    = PackageSettings::get_cloud_package_description(
      PackageSettings::PackageOptions().set_name("sl").set_preview(
        is_preview == IsPreview::yes));

  if (package_description.get_path().is_empty()) {
    SL_PRINTER_TRACE(
      "sl version " + version + " is not available for "
      + sys::System::operating_system_name() + "_" + sys::System::processor());
    return false;
  }

  sl_name = sl_update_name() + OperatingSystem::get_executable_suffix();
  path = binary_directory + "/" + sl_name;

  cloud_service().storage().get_object(
    package_description.get_path(),
    File(File::IsOverwrite::yes, path));

  OperatingSystem::change_mode(path, 0755);

  return true;
}

bool App::execute_update(const var::StringView version) {
  bool result = false;
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("App");

  if (version != "true" && version != "preview") {
    printer().open_command("update");

    if (is_cloud_ready(IsForceDownloadSettings(true)) == false) {
      return false;
    }

    SlPrinter::Output printer_output_guard(printer());
    printer()
      .key("system", OperatingSystem::system_name())
      .key("currentVersion", String(VERSION))
      .key("specifiedVersion", version);

    if (App::download_sl_image(version, IsPreview::no) == false) {
      printer().error("failed to download sl version v" + version).close_fail();
    } else {
      printer()
        .key("apply", "use `slu` to apply the specified version of sl")
        .close_success();
    }

  } else {

    const auto is_preview = version == "preview";
    int update_result
      = App::check_for_update(is_preview ? IsPreview::yes : IsPreview::no);
    if (update_result < 0) {
      result = false;
    } else if (update_result == 0) {
      sys::Version current_version(VERSION);

      printer().open_command("update");

      if (is_cloud_ready(IsForceDownloadSettings(true)) == false) {
        return false;
      }

      PackageSettings package_settings;
      const auto package_description = package_settings.get_package_description(
        PackageSettings::PackageOptions()
          .set_preview(is_preview)
          .set_name("sl"));

      sys::Version latest_version(package_description.get_version());

      SlPrinter::Output printer_output_guard(printer());
      printer()
        .open_object("status")
        .key("system", OperatingSystem::system_name())
        .key("path", OperatingSystem::get_path_to_sl())
        .key("currentVersion", String(VERSION))
        .key("latestVersion", latest_version.string_view())
        .close_success();
    }
  }
  return result;
}

int AppAccess::execute_system_command(
  const sys::Process::Arguments &arguments,
  const var::StringView working_directory) {
  API_ASSERT(arguments.arguments().count() > 0);
  SL_PRINTER_TRACE("executing system process " + arguments.arguments().front());

  auto environment = Process::Environment();
  environment.set_working_directory(working_directory);
  auto process = Process(
    arguments,
    environment);

  API_RETURN_VALUE_IF_ERROR(1);

  // if printer is JSON - encapsulate output in a BASE64 string

  String output;
  printer().open_external_output();
  var::Data buffer(1024 * 1024 * 2);

  auto read_pipe = [&]() {
    const auto result = process.read_standard_output();
    if (result.length()) {
      if (printer().is_json()) {
        output += result;
      } else {
        printf("%s", result.cstring());
      }
    }
  };

  while (process.is_running()) {
    read_pipe();
  }
  read_pipe();

  const auto error_output = process.read_standard_error();

  if (printer().is_json()) {
    printer().key("base64", var::Base64().encode(output));
    if (error_output.is_empty() == false) {
      printer().key("errorBase64", var::Base64().encode(error_output));
    }
  } else {
    if (error_output.is_empty() == false) {
      printer().key("errorOutput", error_output);
    }
  }

  printer().close_external_output();
  return process.status().exit_status();
}

bool AppAccess::is_cloud_ready(
  IsForceDownloadSettings is_force_download_settings,
  IsSuppressError is_suppress_error) {

  printer().open_login();
  if (session_settings().is_cloud_initialized()) {
    return printer().close_login_success(
      cloud_service().cloud().credentials().get_uid());
  }

  if (App::initialize_cloud(is_force_download_settings)) {
    return printer().close_login_fail();
  }

  return printer().close_login_success(
    cloud_service().cloud().credentials().get_uid());
}
