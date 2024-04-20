// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <chrono.hpp>
#include <fs.hpp>
#include <printer.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "Application.hpp"
#include "Cloud.hpp"
#include "settings/HardwareSettings.hpp"
#include "utilities/Packager.hpp"

/*
 * upload windows and mac
 * sl
 * cloud.upload:slmac=host@Z:/git/sl/build_release_link/sl,slwin=host@C:/StratifyLabs-SDK/bin/sl.exe
 *
 */

CloudGroup::CloudGroup() : Connector("cloud", "cloud") {}

bool CloudGroup::execute_command_at(u32 list_offset, const Command &command) {

  if (AppAccess::is_cloud_service_available()) {
    switch (list_offset) {
    case command_login:
      return login(command);
    case command_logout:
      return logout(command);
    case command_refresh:
      return refresh(command);
    case command_install:
      return install(command);
    case command_ping:
      return ping(command);
    case command_sync:
      return sync(command);
    case command_connect:
      return connect(command);
    case command_listen:
      return listen(command);
    case command_remove:
      return remove(command);
    }
  } else {
    if (list_offset == command_install){
      return install(command);
    }
  }

  return false;
}

var::StringViewList CloudGroup::get_command_list() const {

  StringViewList list = {
    "login",
    "logout",
    "refresh",
    "install",
    "ping",
    "sync",
    "connect",
    "listen",
    "remove"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool CloudGroup::login(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      login,
      "logs in to the Stratify cloud using an email/password or uid/token "
      "combination.")
      + GROUP_ARG_OPT(
        email,
        string,
        <none>,
        "The email address to use when logging in.")
      + GROUP_ARG_OPT(
        local,
        bool,
        false,
        "Saves the credentials in the local workspace rather than globally.")
      + GROUP_ARG_OPT(
        password,
        string,
        <none>,
        "The password for the associated email address.")
      + GROUP_ARG_OPT(
        token,
        string,
        <none>,
        "The cloud token for authentication.")
      + GROUP_ARG_OPT(
        uid,
        string,
        <none>,
        "The user ID when using a cloud token to login."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView email = command.get_argument_value("email");
  StringView password = command.get_argument_value("password");
  StringView uid = command.get_argument_value("uid");
  StringView refresh_token = command.get_argument_value("token");
  StringView is_local = command.get_argument_value("local");

  command.print_options(printer());

  SlPrinter::Output printer_output_guard(printer());

  if (email.is_empty() && uid.is_empty()) {
    SL_PRINTER_TRACE("opening browswer for login");
    App::launch_browser_login();
    printer().troubleshoot(
      "Login using `sl cloud.authenticate:uid=<uid>,token=<token>`. The "
      "command can be copied to the clipboard using "
      "from the web application after you have logged it. Paste the command in "
      "the terminal and execute it to store "
      "your credentials.");
  } else if (!email.is_empty()) {

    if (password.is_empty()) {
      APP_RETURN_ASSIGN_ERROR("Must specifiy email and password");
    }

    // login using cloud
    cloud_service().cloud().login(email, password);

  } else if (!uid.is_empty()) {
    if (refresh_token.is_empty()) {
      APP_RETURN_ASSIGN_ERROR("Must specify uid and token");
    }

    SL_PRINTER_TRACE("Setting cloud uid " + uid);
    SL_PRINTER_TRACE("Setting cloud token " + refresh_token);
    cloud_service().cloud().credentials().set_uid(uid).set_refresh_token(
      refresh_token);

    SL_PRINTER_TRACE(
      "refreshing login with "
      + cloud_service().cloud().credentials().get_uid());
    cloud_service().cloud().refresh_login();

  } else {
    APP_RETURN_ASSIGN_ERROR(
      "please use cloud.login:email=<email>,password=<password> OR  "
      "cloud.login:uid=<email>,refresh=<refresh "
      "token>,token=<token>");
    return printer().close_fail();
  }

  SL_PRINTER_TRACE(
    "uid credential is " + cloud_service().cloud().credentials().get_uid());
  SL_PRINTER_TRACE(
    "token credential is " + cloud_service().cloud().credentials().get_token());
  SL_PRINTER_TRACE(
    "refresh token credential is "
    + cloud_service().cloud().credentials().get_refresh_token());

  credentials().copy(cloud_service().cloud().credentials());

  credentials().set_global(is_local != "true");
  save_credentials();

  return is_success();
}

bool CloudGroup::logout(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      logout,
      "removes the login credentials from their source (either global or "
      "within the workspace)."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  command.print_options(printer());

  SlPrinter::Output printer_output_guard(printer());

  if (credentials().get_uid().is_empty()) {
    printer().info("not logged in");
    return is_success();
  }

  printer().key("remove", credentials().path());
  SL_PRINTER_TRACE("removing cloud credentials from workspace");
  cloud_service().cloud().credentials() = Cloud::Credentials();
  credentials().remove();

  return is_success();
}

bool CloudGroup::refresh(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      refresh,
      "refreshes the workspace login. This will happen automatically if the "
      "current login has expired."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  // login using cloud
  SL_PRINTER_TRACE("refreshing cloud login");
  cloud_service().cloud().refresh_login();

  if (is_success()) {
    printer().info("refreshed cloud login");
  }

  return is_success();
}

bool CloudGroup::ping(const Command &command) {

  SL_PRINTER_TRACE_PERFORMANCE();
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();
  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      ping,
      "gets the most recent information for the project ID from the cloud.")
      + GROUP_ARG_OPT(
        build_target,
        string,
        <none>,
        "Build id (default is to ping the project).")
      + GROUP_ARG_OPT(
        identifier_id,
        string,
        <none>,
        "The ID of the project to ping.")
      + GROUP_ARG_OPT(url, string, <none>, "The url of the project to ping."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView identifier = command.get_argument_value("identifier");
  StringView build_id = command.get_argument_value("build");
  StringView url = command.get_argument_value("url");

  command.print_options(printer());

  if (!is_cloud_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  if (!identifier.is_empty()) {

    if (!build_id.is_empty()) {
      Build build(
        Build::Construct().set_project_id(identifier).set_build_id(build_id));

      if (build.is_existing()) {
        printer().open_object(build_id);
        printer() << build.to_object();
        printer().close_object();
      } else {
        APP_RETURN_ASSIGN_ERROR("`buildid` does not exist");
      }
    } else {

      Project project(identifier);

      if (project.is_existing()) {
        {
          api::ErrorGuard error_guard;
          project.remove_readme();
        }
        printer().active_printer().object(
          project.get_document_id(),
          project.to_object());
      } else {
        APP_RETURN_ASSIGN_ERROR("project `id` does not exist");
      }
    }

  } else if (!url.is_empty()) {

    Build build(Build::Construct().set_url(url));

    if (build.is_existing()) {
      printer().open_object(build.get_document_id());
      build.remove_build_image_data();
      printer() << build.to_object();
      printer().close_object();
    } else {
      APP_RETURN_ASSIGN_ERROR("build `id` does not exist");
    }

  } else {
    APP_RETURN_ASSIGN_ERROR("must specify 'id' or 'url'");
  }

  return is_success();
}

bool CloudGroup::sync(const Command &command) {


  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();
  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      sync,
      "synchronizes datum items stored on the device to the cloud.")
      + GROUP_ARG_OPT(
        clean,
        bool,
        false,
        "Whether or not to delete the log file.")
      + GROUP_ARG_OPT(
        path_p,
        string,
        <auto>,
        "The path to a JSON file that includes datum objects to sync with the "
        "cloud."));
  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  command.print_options(printer());

  // pull the reports that are stored on the device and publish them

  return is_success();
}

bool CloudGroup::connect(const Command &command) {


  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();
  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      connect,
      "connects the workspace to an active job server. Commans will be posted "
      "to the job id while the job server is active.")
      + GROUP_ARG_REQ(
        job_id,
        string,
        <job id>,
        "The job id to connect to. A job server can be created using `sl "
        "cloud.listen:job`. Use `sl cloud.connect:job=none` to disconnect.")
      + GROUP_ARG_OPT(
        report,
        boolean,
        false,
        "Create a report for each set of commands that is executed.")
      + GROUP_ARG_OPT(
        permissions,
        string,
        <auto>,
        "Specify the permissions for any reports that are created.")
      + GROUP_ARG_OPT(
        team,
        string,
        <none>,
        "Specify the team that owns the report (default is none).")
      + GROUP_ARG_OPT(
        timeout,
        integer,
        <indefinite>,
        "Timeout in seconds to allow for the job execution."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView job_id = command.get_argument_value("job");
  StringView permissions = command.get_argument_value("permissions");
  StringView team = command.get_argument_value("team");
  StringView timeout = command.get_argument_value("timeout");

  if (job_id.is_empty()) {
    job_id = "none";
  }

  command.print_options(printer());

  if (!is_cloud_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  if (job_id.is_empty() || job_id == "none") {
    workspace_settings().set_job_info(
      JobInfo()
        .set_job_id("")
        .set_timeout(timeout.to_integer())
        .set_cloud_job(
          CloudJobInput().set_permissions(permissions).set_team(team)));

  } else {

    if (!Job().ping(job_id)) {
      printer().troubleshoot(
        "You can create a job server using the `sl cloud.listen` command. You "
        "can use `sl --help=cloud.listen` for more details");
      APP_RETURN_ASSIGN_ERROR("failed to ping job " + job_id);
    }

    workspace_settings().set_job_info(
      JobInfo()
        .set_job_id(job_id)
        .set_timeout(timeout.to_integer())
        .set_cloud_job(
          CloudJobInput().set_permissions(permissions).set_team(team)));
  }

  workspace_settings().save();

  return is_success();
}

GroupFlags::ExecutionStatus
CloudGroup::publish_job(const var::String &command) {

  printer().open_command("cloud.publish.job");

  JobInfo job_info = workspace_settings().job_info();

  SlPrinter::Output printer_output_guard(printer());
  {
    Printer::Object pj(printer().output(), "job");
    printer().output_key("id", job_info.get_job_id());
    printer().output_key("command", command);
    printer().key_bool("report", job_info.cloud_job().is_report());
    printer().output_key("permissions", job_info.cloud_job().get_permissions());
    if (!job_info.cloud_job().get_team().is_empty()) {
      printer().output_key("team", job_info.cloud_job().get_team());
    }
  }

  JsonValue output
    = Job().publish(job_info.cloud_job(), job_info.get_timeout() * 1_seconds);

  if (output.is_null()) {
    workspace_settings().job_info().set_job_id("");
    workspace_settings().save();
    printer().error("failed to publish job");
    return ExecutionStatus::failed;
  } else {
    CloudJobOutput job_output(output.to_object());

    printer().close_success();

    if (job_info.get_cloud_job().is_report()) {
      // get the report
    } else {
      printf("%s", String(Base64().decode(job_output.get_bash())).cstring());
    }
  }

  return ExecutionStatus::success;
}

bool CloudGroup::listen(const Command &command) {


  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();
  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      connect,
      "connects the workspace to an active job server. Commans will be posted "
      "to the job id while the job server is active.")
      + GROUP_ARG_OPT(
        job_id,
        string,
        <auto>,
        "The job id to connect to. Post jobs to the server using `sl "
        "cloud.connect:job=<job_id>`.")
      + GROUP_ARG_OPT(
        timeout,
        integer,
        <none>,
        "Set the timeout in seconds to stop listening.")
      + GROUP_ARG_OPT(
        permissions,
        string,
        private,
        "Specify the permissions for the job server.")
      + GROUP_ARG_OPT(
        team,
        string,
        <none>,
        "Team that has access to the job server."));
  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView job_id = command.get_argument_value("job");
  StringView timeout = command.get_argument_value("timeout");
  StringView permissions = command.get_argument_value("permissions");
  StringView team_id = command.get_argument_value("team");

  command.print_options(printer());

  if (!is_cloud_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object("job");

  if (permissions == "private" && team_id.is_empty()) {
    APP_RETURN_ASSIGN_ERROR(
      "if `permissions` is `private`, you must specify a `team` value");
  }

  SL_PRINTER_TRACE("set job server characteristics");
  Job::Server job_server;
  job_server.set_context(this).set_callback(handle_cloud_listen_function);

  SL_PRINTER_TRACE("create job server");

  m_job_server = &job_server;

  SL_PRINTER_TRACE("create job server id");
  job_server.start(
    "sl",
    Job().set_id(job_id).set_permissions(permissions).set_team_id(team_id));

  m_job_server = nullptr;

  if (is_success()) {
    printer().output_key("id", job_server.id());
    printer().info(
      "push `ctrl+c` to stop listening. It will up to 20 seconds to stop.");
    printer().close_success();
  }

  // SL_PRINTER_TRACE("database traffic " + cloud().database_traffic());

  // pull the reports that are stored ont the device and publish them

  printer().open_command("cloud.listen.finalize");

  return is_success();
}

json::JsonValue CloudGroup::handle_cloud_listen(
  const StringView type,
  const json::JsonValue &value) {

  API_ASSERT(m_job_server != nullptr);

  if (session_settings().is_interrupted()) {
    m_job_server->set_stop();
    return JsonNull();
  }

  if (type != "sl") {
    return JsonNull();
  }

  CloudJobInput input(value.to_object());
  CloudJobOutput output;
  if (!input.get_command().is_empty()) {

    if (input.is_report()) {
      printer().set_insert_codefences();
    } else {
      // disable reporting
    }

    const auto command = String(Base64().decode(input.get_command()));
    printer().open_command("cloud.listen");
    SlPrinter::Output printer_output_guard(printer());
    printer().open_object("job");
    printer().key("id", m_job_server->id());
    printer().key("command", command);
    printer().close_success();

    // refuse some commands (like fs.execute) -- add a way to set a filter

    printer().set_report(StringView());

    Group::execute_compound_command(command);

    Report execution_report;
    fs::DataFile report_file;

    if (input.is_report()) {

      if (!input.get_permissions().is_empty()) {
        execution_report.set_permissions(input.get_permissions());
      }

      if (!input.get_team().is_empty()) {
        execution_report.set_team_id(input.get_team());
      }

      printer().open_command("Report");
      if (!is_cloud_ready()) {
        printer().close_fail();
      } else {

        SlPrinter::Output printer_output_guard(printer());
        execution_report.save(report_file);
        printer().close_output("report", execution_report.id());
#if NOT_BUILDING
        output.set_report(report_id);
#endif
      }
    } else {
      output.set_bash(Base64().encode(printer().report()));
    }
  }
  return std::move(output);
}

bool CloudGroup::install(const Command &command) {

  GROUP_ADD_SESSION_REPORT_TAG();

  printer().open_command(GROUP_COMMAND_NAME);

  // install the project -- app or BSP
  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      install,
      "installs an application or OS package from the cloud on to a "
      "connected "
      "device.")
      + GROUP_ARG_OPT(compiler, bool, false, "Install the compiler")
      + GROUP_ARG_OPT(hash, string, <none>, "Sha256 hash for downloaded image")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "Download but don't extract the compiler")
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
        architecture_arch,
        string,
        <auto>,
        "Architecture to install (required for applications being saved to "
        "local host).")
      + GROUP_ARG_OPT(build_target, string, release, "Build name to install.")
      + GROUP_ARG_OPT(
        clean,
        bool,
        true,
        "other copies of the application are deleted before installation.")
      + GROUP_ARG_OPT(
        keys_key,
        string,
        <none>,
        "key id to use for signing the firmware.")
      + GROUP_ARG_OPT(
        password_pwd,
        string,
        <none>,
        "password to decrypt the private key for code signing.")
      + GROUP_ARG_OPT(
        reconnect,
        bool,
        true,
        "try to reconnect after installing an OS image.")
      + GROUP_ARG_OPT(
        delay,
        int,
        500,
        "number of milliseconds to delay between reconnect tries.")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        </ app>,
        "install location for applications or specify a host path to save as "
        "a "
        "local file.")
      + GROUP_ARG_OPT(
        external_ext,
        bool,
        false,
        "install the code and data in external RAM (ignored if *ram* is "
        "*false*).")
      + GROUP_ARG_OPT(
        identifier_id,
        string,
        <id>,
        "cloud ID to download and install.")
      + GROUP_ARG_OPT(
        key,
        bool,
        false,
        "insert a random key in the binary if there is room in the image for "
        "a "
        "secret key (only works for OS projects). If the thing already has a "
        "key it will be preserved.")
      + GROUP_ARG_OPT(
        ram,
        bool,
        false,
        "install the application in ram (no effect if the id is an OS "
        "package).")
      + GROUP_ARG_OPT(
        rekey,
        bool,
        false,
        "insert a new random key in the binary. If a key already exists it "
        "will be replaced.")
      + GROUP_ARG_OPT(
        synchronize_sync,
        bool,
        false,
        "Synchronize the installation with the cloud.")
      + GROUP_ARG_OPT(
        retry,
        int,
        50,
        "number of times to try to connect or reconnect when installing an "
        "OS "
        "package.")
      + GROUP_ARG_OPT(
        startup,
        bool,
        false,
        "an installed application will run at startup if supported on the "
        "target filesystem.")
      + GROUP_ARG_OPT(
        update,
        bool,
        false,
        "check for os and app updates of the attached device.")
      + GROUP_ARG_OPT(
        os,
        bool,
        false,
        "check for os updates of the attached device (installed apps may be "
        "lost).")
      + GROUP_ARG_OPT(
        application_app,
        bool,
        false,
        "check for os updates of the attached device (installed apps may be "
        "lost).")
      + GROUP_ARG_OPT(
        directories,
        string,
        <'app/flash|/home|/home/bin'>,
        "? separated list of directories to search for application updates.")
      + GROUP_ARG_OPT(
        name,
        string,
        <auto>,
        "name to embed in application (will be determined automatically if not "
        "provided)")
      + GROUP_ARG_OPT(
        suffix,
        string,
        <none>,
        "Suffix appended to the name of the target destination (for creating "
        "multiple copies of the same application).")
      + GROUP_ARG_OPT(
        team,
        string,
        <public>,
        "the team ID of the project to install (default is to install a "
        "public "
        "project).")
      + GROUP_ARG_OPT(
        tightlycoupled_tc,
        bool,
        false,
        "install the code and data in tightly coupled RAM (ignored if *ram* "
        "is "
        "*false*).")
      + GROUP_ARG_OPT(
        url,
        string,
        <none>,
        "url to an exported application or OS package (or compiler).")
      + GROUP_ARG_OPT(
        version_v,
        string,
        <latest>,
        "version to install (default is latest)."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView id = command.get_argument_value("identifier");
  StringView compiler = command.get_argument_value("compiler");
  StringView hash = command.get_argument_value("hash");
  StringView clean = command.get_argument_value("clean");
  StringView version = command.get_argument_value("version");
  StringView dryrun = command.get_argument_value("dryrun");
  StringView url = command.get_argument_value("url");
  StringView destination = command.get_argument_value("destination");
  StringView update = command.get_argument_value("update");
  StringView update_os = command.get_argument_value("os");
  StringView update_apps = command.get_argument_value("application");
  StringView directories = command.get_argument_value("directories");

  command.print_options(printer());

  if (!is_cloud_service_available() && !is_cloud_ready()) {
    return printer().close_fail();
  }

  const bool is_command_bad = id.is_empty() && url.is_empty()
                              && update.is_empty() && compiler != "true";
  if (is_command_bad) {
    SlPrinter::Output printer_output_guard(printer());
    APP_RETURN_ASSIGN_ERROR("either `id` or `url` or `update` is required");
  }

  const bool is_update = (update_os == "true") || (update_apps == "true");

  const Link::Path destination_link_path(destination, connection()->driver());

  SL_PRINTER_TRACE("destination `" | destination | "`");

  if (
    (compiler != "true")
    && (destination.is_empty() || destination_link_path.is_device_path() || is_update)) {
    if (!is_connection_ready(IsBootloaderOk(true))) {
      return printer().close_fail();
    }
  }

  if (directories.is_empty()) {
    directories = "/app/flash?/home?/home/bin";
  }

  SlPrinter::Output printer_output_guard(printer());

  if (compiler == "true") {

    if( destination.is_empty() ){
      APP_RETURN_ASSIGN_ERROR("`destination` must be provided when installing the compiler (usually `SDK/local`)");
    }

    Packager()
      .set_keep_temporary(clean != "true")
      .deploy(Packager::DeployOptions()
                .set_hash(hash)
                .set_extract(dryrun != "true")
                .set_url(url)
                .set_name("compiler")
                .set_version(version)
                .set_destination_directory_path(destination));

    return is_success();
  } else if (!is_cloud_service_available()){
    APP_RETURN_ASSIGN_ERROR("`cloud.install:compiler` is the only option available with this build");
  }

  printer().open_object(
    (update == "false") ? (id.is_empty() ? url : id) : String("update"));

  if (!id.is_empty()) {
    session_settings().add_project_tag(id);
  }

  Installer installer(connection());

  installer.install(
    get_installer_options(command)
      .set_update_os(update == "true" && update_os == "true")
      .set_update_apps(update == "true" && update_apps == "true")
      .set_update_app_directories(directories));

  return is_success();
}

var::Vector<CloudAppUpdate> CloudGroup::check_for_app_updates(
  PathString path,
  IsForceReinstall is_reinstall) {


  auto list = Link::FileSystem(connection()->driver())
                .read_directory(path, Dir::IsRecursive(false));

  var::Vector<CloudAppUpdate> result;

  for (const auto &item : list) {
    const PathString app_path = PathString(path) / item;

    if (item.string_view().at(0) != '.') {

      SL_PRINTER_TRACE("load application info for " + app_path);

      Appfs::Info info = Appfs(connection()->driver()).get_info(app_path);
      if (info.is_valid()) { // non executable files will not have valid info
        // (like settings.json)
        printer().open_object(app_path.cstring());

        if (info.id().is_empty()) {
          printer().warning("application id is missing");
        } else {

          sys::Version installed_version
            = sys::Version::from_u16(info.version());

          Project project(info.id());

          if (is_error()) {
            printer().close_object(); // app path
            return result;
          }

          sys::Version cloud_version(project.get_version());

          printer().key("latest", cloud_version.string_view());
          printer().key("installed", installed_version.string_view());

          if (
            (cloud_version > installed_version)
            || is_reinstall == IsForceReinstall::yes) {

            result.push_back(CloudAppUpdate(
              PathString(path),
              NameString("release"),
              project,
              info));

          } else {
            printer().info(
              "latest version `" + installed_version.string_view()
              + "` is already installed");
          }
        }

        printer().close_object(); // app path
      }
    }
  }

  return result;
}

bool CloudGroup::remove(const Command &command) {

  SL_PRINTER_TRACE_PERFORMANCE();
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();
  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(remove, "removes the project and/or build specified.")
      + GROUP_ARG_OPT(
        identifier_id,
        string,
        <from project>,
        "The ID of the project to remove.")
      + GROUP_ARG_OPT(
        build,
        string,
        <none>,
        "Build id to remove (if not specified all builds and the project are "
        "removed).")
      + GROUP_ARG_OPT(
        path,
        string,
        <none>,
        "path to the local project being modified (providing this keeps the "
        "local settings sync'd)."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView id = command.get_argument_value("identifier");
  StringView build_id = command.get_argument_value("build");
  StringView path = command.get_argument_value("path");

  command.print_options(printer());

  if (!is_cloud_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  const auto project_file_path
    = FilePathSettings::project_settings_file_path(path);

  if (!path.is_empty()) {

    if (
      !FileSystem().exists(project_file_path)
      || !JsonDocument::is_valid(File(project_file_path))) {
      APP_RETURN_ASSIGN_ERROR("path does not refer to a valid project");
    }

  } else if (id.is_empty()) {
    APP_RETURN_ASSIGN_ERROR("`id` or `path` must be specified");
  }

  const Document::Id identifier
    = id.is_empty()
        ? Project().import_file(File(project_file_path)).get_document_id()
        : id;

  const auto id_path = build_id.is_empty()
                         ? identifier.string_view()
                         : (identifier.string_view() / build_id).string_view();

  printer().open_object(id_path.is_empty() ? path : id_path);

  Project project(identifier);
  if (!project.is_existing()) {
    APP_RETURN_ASSIGN_ERROR("project does not exist in the cloud");
  }

  if (!build_id.is_empty()) {
    Build build(
      Build::Construct().set_project_id(identifier).set_build_id(build_id));

    if (!build.is_existing()) {
      APP_RETURN_ASSIGN_ERROR("build does not exist");
    }

    if (is_success()) {
      const auto build_list = project.get_build_list();
      auto updated_build_list = build_list;
      int offset = 0;
      for (const auto &build : build_list) {
        if (build.key() == build_id) {
          updated_build_list.remove(offset);
          break;
        }
        offset++;
      }
      project.set_build_list(updated_build_list);
      project.save();
    }

  } else {

    if (project.is_existing()) {

      // remove all the builds
      const auto build_list = project.get_build_list();
      for (const auto &build : build_list) {
        printer().key("removeBuild", (identifier / build.key()));
        Build(Build::Construct()
                .set_project_id(identifier)
                .set_build_id(build.key()))
          .remove();
      }

      printer().key("removeProject", identifier);

      project.remove();

      {
        api::ErrorContext ec;
        project.set_document_id("").remove_build_list();
      }
    }

    if (!path.is_empty() && is_success()) {
      project.export_file(File(File::IsOverwrite::yes, project_file_path));
    }
  }

  return is_success();
}

void CloudGroup::apply_app_updates(const CloudAppUpdate &update) {


  const PathString app_path
    = PathString(update.path()) / update.project().get_name();

  Printer::Object(printer().active_printer(), app_path);

  const Appfs::Info info = Appfs(connection()->driver()).get_info(app_path);
  // remove the application if it is present -- may have been deleted by the
  // OS update
  Link::FileSystem(connection()->driver()).remove(app_path);

  SL_PRINTER_TRACE(
    "installing application " + update.project().get_document_id());

  Installer installer(connection());

  installer.install(
    Installer::Install()
      .set_project_id(update.project().get_document_id())
      .set_build_name("release")
      .set_version(update.project().get_version())
      .set_destination(update.path())
      .set_flash(update.info().is_flash())
      .set_ram_size(update.info().ram_size())
      .set_startup(update.info().is_startup())
      .set_external_code(update.info().is_code_external())
      .set_external_data(update.info().is_data_external())
      .set_tightly_coupled_code(update.info().is_code_tightly_coupled())
      .set_tightly_coupled_data(update.info().is_data_tightly_coupled()));
}

bool CloudGroup::check_for_update() { return false; }

bool CloudGroup::is_project_path_valid(const var::StringView path) {


  PathString project_path = PathString(path) / "StratifySettings.json";

  json::JsonDocument json_document;
  JsonValue json_value = json_document.load(File(project_path));

  if (
    json_value.is_object() && json_value.to_object().at(("name")).is_valid()) {
    return true;
  }

  return false;
}

int CloudGroup::refresh(bool is_force_refresh) { return -1; }
