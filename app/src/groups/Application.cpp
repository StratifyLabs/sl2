// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <chrono.hpp>
#include <fs.hpp>
#include <printer.hpp>
#include <sys.hpp>

#include "Application.hpp"
#include "Task.hpp"
#include "settings/TestSettings.hpp"

Application::Application(Terminal &terminal)
  : Connector("application", "app"), m_terminal(terminal) {}

bool Application::execute_command_at(u32 list_offset, const Command &command) {

  switch (list_offset) {
  case command_install:
    return install(command);
  case command_run:
    return run(command);
  case command_publish:
    return publish(command);
  case command_clean:
    return clean(command);
  case command_ping:
    return ping(command);
  case command_profile:
    return profile(command);
  }

  return false;
}

var::StringViewList Application::get_command_list() const {
  StringViewList list
    = {"install", "run", "publish", "clean", "ping", "profile"};

  API_ASSERT(list.count() == command_total);

  return list;
}

bool Application::install(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      install,
      "installs an application that was built on the host computer to a "
      "connected device.")
      + GROUP_ARG_OPT(
        arguments_args,
        string,
        <none>,
        "specifies arguments to pass to the application (use with 'run').")
      + GROUP_ARG_OPT(
        authenticated_auth,
        bool,
        false,
        "install the application with the authenticated flag set.")
      + GROUP_ARG_OPT(
        build_target,
        string,
        release,
        "is usually set to`release` or `debug`")
      + GROUP_ARG_OPT(
        architecture_arch,
        string,
        <auto>,
        "CPU architecture of the target (e.g. `v7em_f4sh`")
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
        name,
        string,
        <auto>,
        "name to embed in the binary (will be automatically determined if not provided).")
      + GROUP_ARG_OPT(
        clean,
        bool,
        true,
        "other copies of the app are deleted before installation.")
      + GROUP_ARG_OPT(
        orphan_nowait,
        bool,
        false,
        "mark the application is being orphaned (no need to `wait()`).")
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
        ramsize_datasize,
        int,
        <default>,
        "amount of RAM in bytes to use for data memory (default is to use "
        "value specified by the developer).")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <auto>,
        "Installation destination")
      + GROUP_ARG_OPT(
        external_ext,
        bool,
        <false>,
        "install the code and data in external RAM (ignored if *ram* is "
        "*false*).")
      + GROUP_ARG_OPT(
        externalcode,
        bool,
        false,
        "Install the code in external memory.")
      + GROUP_ARG_OPT(
        externaldata,
        bool,
        false,
        "Install the data in external RAM.")
      + GROUP_ARG_OPT(
        force,
        bool,
        false,
        "Install over a currently running application without killing first.")
      + GROUP_ARG_OPT(
        kill,
        bool,
        true,
        "Kill the application before installing, otherwise installation is "
        "aborted if the application is running.")
      + GROUP_ARG_REQ(
        path_p,
        string,
        <path>,
        "The relative path to the application project folder on the host "
        "computer.")
      + GROUP_ARG_OPT(
        ram,
        bool,
        <auto>,
        "Install the code in RAM rather than flash (can't be used with "
        "'startup').")
#if NOT_IMPLEMENTED_YET
      + GROUP_ARG_OPT(
        recursive_r,
        bool,
        false,
        "Search path recursively and install all apps that are found.")
#endif
      + GROUP_ARG_OPT(
        run,
        bool,
        false,
        "Application is run after it has been installed.")
      + GROUP_ARG_OPT(
        startup,
        bool,
        false,
        "Install the program so that it runs when the OS starts up (can't be "
        "used with 'ram').")
      + GROUP_ARG_OPT(
        suffix,
        string,
        <none>,
        "Suffix to be appended to the application name (for creating multiple "
        "copies of the same application).")
      + GROUP_ARG_OPT(
        terminal_term,
        bool,
        false,
        "Run the terminal while the application is running (used with 'run').")
      + GROUP_ARG_OPT(
        tightlycoupled_tc,
        bool,
        <false>,
        "Install the code and data in tightly coupled RAM (ignored if *ram* is "
        "*false*).")
      + GROUP_ARG_OPT(
        tightlycoupledcode_tcc,
        bool,
        false,
        "Install the code in tightly coupled memory.")
      + GROUP_ARG_OPT(
        tightlycoupleddata_tcd,
        bool,
        false,
        "Install the data in tightly coupled RAM."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto path = command.get_argument_value("path");
  const auto is_terminal = command.get_argument_value("terminal");
  const auto destination_arg = command.get_argument_value("destination");
  const auto run = command.get_argument_value("run");
  const auto run_arguments = command.get_argument_value("arguments");
  const auto sign_key = command.get_argument_value("signkey");

  StringView recursive = command.get_argument_value("recursive");
  recursive = "false";

  const StringView destination
    = destination_arg.is_empty() ? "/app" : destination_arg;

  command.print_options(printer());

  if (!sign_key.is_empty()) {
    if (!is_cloud_ready()) {
      return printer().close_fail();
    }
  }

  if (!workspace_settings().get_sign_key().is_empty()) {
    if (!is_cloud_ready()) {
      return printer().close_fail();
    }
  }

  if (
    Link::Path(destination, connection()->driver()).is_device_path()
    && !is_connection_ready()) {
    return printer().close_fail();
  }

  NameString name;
  {
    SlPrinter::Output printer_output_guard(printer());

    if (recursive == "true" && run == "true") {
      APP_RETURN_ASSIGN_ERROR(
        "`recursive` and `run` options cannot be used together");
    }

    PathList path_list;
    if (recursive == "true") {
      printer().open_object("recursive");
      // search for binaries that match the arch
      const auto dir_list = FileSystem().read_directory(path);
      for (const auto &dir : dir_list) {
        const auto dir_path = path / dir;
        const auto info = FileSystem().get_info(dir_path);

        const auto settings_path
          = FilePathSettings::project_settings_file_path(dir_path);

        if (
          info.is_directory() && FileSystem().exists(settings_path)
          && JsonDocument::is_valid(File(settings_path))) {
          printer().key(dir_path, "queued");
          path_list.push_back(dir_path);
        }
      }
      printer().close_object();
    } else {
      path_list.push_back(path);
    }

    for (const auto &entry_path : path_list) {
      bool is_source_a_directory = true;
      {
        FileInfo source_directory_info = FileSystem().get_info(entry_path);
        if (!source_directory_info.is_valid()) {

          printer().troubleshoot("When specifying the entry_path, the entry_path is always relative to the current directory. The entry_path can point to a "
                            "directory that contains a valid `"
						+ Project::file_name()
                                                + "` file that describes an application project or it can point to an application binary image file.");
          APP_RETURN_ASSIGN_ERROR(
          entry_path | " does not refer to a valid directory or binary "
          "image file.");

        } else if (!source_directory_info.is_directory()) {
          is_source_a_directory = false;
        }
      }

      StringView binary_image_path;
      StringView project_path;
      if (is_source_a_directory) {
        project_path = entry_path;
        SL_PRINTER_TRACE("try to import project " | project_path);
        name = fs::Path::name(project_path);

        Project project = Project().import_file(
          File(FilePathSettings::project_settings_file_path(project_path)));

        if (!project.get_document_id().is_empty()) {
          session_settings().add_project_tag(project);
        }
      } else {
        binary_image_path = entry_path;
        SL_PRINTER_TRACE("use binary entry_path " | binary_image_path);
        Appfs::Info info = Appfs().get_info(binary_image_path);
        name = info.name();
      }

      SL_PRINTER_TRACE("project entry_path is `" | project_path | "`");
      SL_PRINTER_TRACE("binary entry_path is `" | binary_image_path | "`");

      {
        printer().open_object(fs::Path::name(entry_path));

        const auto project_file_path = project_path / Project::file_name();

        if (
      ! project_path.is_empty() &&
      (! FileSystem().exists(project_file_path)
       || !JsonDocument::is_valid(
         File(project_file_path),
         &printer().active_printer()))) {
          APP_RETURN_ASSIGN_ERROR(
            "project file " | project_file_path.string_view()
            | " does not exist");
        }

        if (!project_path.is_empty()) {
          session_settings().add_project_tag(
            Project().import_file(File(project_file_path)));
        }

        Installer(connection())
          .install(get_installer_options(command)
                     .set_project_path(project_path)
                     .set_binary_path(binary_image_path)
                     .set_application(true));
      }

      API_RETURN_VALUE_IF_ERROR(false);
    }
  }

  if ((run == "true" && recursive != "true") || !run_arguments.is_empty()) {
    GeneralString args = "run:path=" | name;
    if (is_terminal == "true") {
      args |= ",terminal=true";
    }
    if (!run_arguments.is_empty()) {
      args |= ",args=" | run_arguments;
    }

    Command run_command(Command::Group(get_name()), args);

    SL_PRINTER_TRACE("running as " + args);
    if (!this->run(run_command)) {
      return false;
    }
  }

  return true;
}

void Application::kill(Connection *connection, const var::StringView name) {

  TaskManager task_manager("", connection->driver());

  int app_pid = task_manager.get_pid(name);
  if (app_pid < 0) {
    return;
  }

  task_manager.kill_pid(app_pid, LINK_SIGINT);

  // give a little time for the program to shut down
  SL_PRINTER_TRACE("Wait for killed program to stop");
  int retries = 0;
  while ((task_manager.get_pid(name) > 0) && (retries < 5)) {
    retries++;
    chrono::wait(100_milliseconds);
  }
}

bool Application::run(const Command &command) {
  // run with arguments
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(run, "runs an application on a connected device.")
      + GROUP_ARG_OPT(
        arguments_args,
        string,
        <none>,
        "Arguments to pass to the application.")
      + GROUP_ARG_REQ(
        path_p,
        string,
        <path>,
        "The path to the application to execute (if just a name is provided, "
        "/app is searched for a match).")
      + GROUP_ARG_OPT(
        terminal_term,
        bool,
        false,
        "The terminal will run while the application runs."));

  if (!command.is_valid(reference, printer())) {
    printer().open_command(GROUP_COMMAND_NAME);
    return printer().close_fail();
  }

  const auto path = command.get_argument_value("path");
  const auto app_arguments = command.get_argument_value("arguments");
  const auto is_terminal = command.get_argument_value("terminal");

  if (is_terminal == "true") {
    // start the terminal now
    const String run_terminal_string = "run:while=" + Path::name(path);
    Command run_terminal_command(
      Command::Group("terminal"),
      run_terminal_string);
    if (!m_terminal.execute_run(run_terminal_command)) {
      return false;
    }
  }

  printer().open_command(GROUP_COMMAND_NAME);
  command.print_options(printer());

  if (!is_connection_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(path);

  Link::Path link_path(path, connection()->driver());

  if (link_path.is_host_path()) {
    APP_RETURN_ASSIGN_ERROR("cannot execute from `host` path");
  }

  PathString exec_path = link_path.path();

  if (link_path.path().find("/app/") != 0) {

    PathString search_path = PathString("/app/flash") / link_path.path();
    Appfs::Info info = Appfs(connection()->driver()).get_info(search_path);

    if (is_error()) {
      API_RESET_ERROR();

      search_path = PathString("/app/ram") / link_path.path();
      info = Appfs(connection()->driver()).get_info(search_path);

      if (!info.is_valid()) {
        APP_RETURN_ASSIGN_ERROR(
          "Can't find executable application `" + link_path.path()
          + "` in /app/flash or /app/ram");
      } else {
        exec_path = search_path;
      }
    } else {
      session_settings().add_project_tag(info.id());
      exec_path = search_path;
    }
  }

  if (!app_arguments.is_empty()) {
    exec_path.append(" ").append(app_arguments);
  }

  printer().key("exec", exec_path);
  connection()->run_app(exec_path);

  return is_success();
}

bool Application::publish(const Command &command) {
  // run with arguments
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      publish,
      "publishes a version of the application to the Stratify Cloud. The first "
      "time app.publish is used on a project "
      "the cloud provisions an ID which needs to be built into the application "
      "before app.publish is called again with "
      "the changes specified.")
      + GROUP_ARG_OPT(changes, string, <none>, "deprecated")
      + GROUP_ARG_OPT(
        header,
        bool,
        false,
        "publish the project settings to the `sl_config.h` header file.")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "List what will be uploaded without uploading")
      + GROUP_ARG_OPT(
        fork,
        bool,
        false,
        "Forks off of an existing application. This should be true if another "
        "user already published this "
        "application.")
      + GROUP_ARG_OPT(
        path_p,
        string,
        <all>,
        "Path to the application that will be published. If not provided, all "
        "application projects in the workspace "
        "are published.")
      + GROUP_ARG_OPT(roll, bool, false, "Rolls the version number."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView path = command.get_argument_value("path");
  const StringView header = command.get_argument_value("header");

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

  if (!publish_project(command, Build::Type::application)) {
    return printer().close_fail();
  }

  return is_success();
}

void Application::clean_directory(const var::StringView path) {
  API_RETURN_IF_ERROR();
  Printer::Object po(printer().output(), path);

  struct ThreadArgument {
    ThreadArgument() : condition(mutex) {}
    Printer *printer = nullptr;
    Mutex mutex;
    Cond condition;
    bool is_clean_complete = false;
  };

  Link::FileSystem fs(connection()->driver());
  if(!fs.exists(path)){
    API_RETURN_ASSIGN_ERROR(path | " does not exists on the device", EINVAL);
  }

  ThreadArgument thread_argument;
  thread_argument.printer = &printer().output();
  Thread progress_thread
    = Thread(
        Thread::Attributes().set_detached(),
        Thread::Construct()
          .set_argument(&thread_argument)
          .set_function([](void *args) -> void * {
            auto *thread_argument
              = reinterpret_cast<ThreadArgument *>(args);
            Printer *printer = thread_argument->printer;
            printer->set_progress_key("clean");
            bool is_complete = false;
            int count = 0;
            do {
              printer->update_progress(
                count++,
                api::ProgressCallback::indeterminate_progress_total());
              {
                Mutex::Guard mg(thread_argument->mutex);
                is_complete = thread_argument->is_clean_complete;
              }
              wait(250_milliseconds);
            } while (!is_complete);
            printer->set_progress_key("progress");
            printer->update_progress(0, 0);
            thread_argument->condition.signal();
            return nullptr;
          }))
        .move();

  const auto list = fs.read_directory(path, fs::Dir::IsRecursive::no);
  PathList path_list;
  for (const auto &item : list) {
    const auto remove_path = PathString(path) / item;
    if (item.length() && item.string_view().at(0) != '.') {
      fs.remove(remove_path);
      if (is_success()) {
        path_list.push_back(remove_path);
      }
    }
  }

  {
    Mutex::Guard mg(thread_argument.mutex);
    thread_argument.is_clean_complete = true;
    thread_argument.condition.wait();
  }

  wait(100_milliseconds);

  if (path_list.count() > 0) {
    printer().object("removed", path_list);
  }
}

bool Application::clean(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      clean,
      "deletes all applications from the specified location.")
      + GROUP_ARG_OPT(
        path_p,
        string,
        </ app / ram and / app / flash>,
        "Path to clean (default is /app/flash and /app/ram)"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView path = command.get_argument_value("path");

  command.print_options(printer());

  if (!is_connection_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  if (path.is_empty()) {

    clean_directory("/app/flash");

    clean_directory("/app/ram");
  } else {
    SL_PRINTER_TRACE("clean " + path + " directory");
    Link::Path link_path(path, connection()->driver());
    clean_directory(link_path.path());
  }

  return is_success();
}

bool Application::ping(const Command &command) {


  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      ping,
      "gets information about an application from the device.")
      + GROUP_ARG_REQ(
        path_p,
        string,
        <path>,
        "Path to the application to ping or a folder containing applications"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto path = command.get_argument_value("path");
  const Link::Path link_path(path, connection()->driver());

  command.print_options(printer());

  if (link_path.is_device_path() && !is_connection_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  PathList app_list;
  Link::FileSystem link_fs(link_path.driver());
  FileInfo file_info = link_fs.get_info(link_path.path());

  if (file_info.is_directory()) {
    if (link_path.is_host_path()) {

      Project project = Project().import_file(
        File(FilePathSettings::project_settings_file_path(link_path.path())));

      session_settings().tag_list().push_back(
        String("project:" + project.get_document_id()));

      printer().object(link_path.path_description(), project);
      return is_success();
    }

    app_list
      = link_fs.read_directory(link_path.path(), fs::Dir::IsRecursive(false));

  } else if (!file_info.is_valid()) {
    APP_RETURN_ASSIGN_ERROR(
      "application at " + link_path.path_description() + " does not exist");
  } else {
    app_list.push_back(link_path.path());
  }

  for (u32 i = 0; i < app_list.count(); i++) {
    PathString name;
    const PathString item_path = [&]() {
      PathString result;
      if (file_info.is_directory()) {
        result = PathString(link_path.path()) / app_list.at(i);
        if (app_list.at(i).string_view().find((".")) == 0) {
          return PathString();
        }
      }

      return result;
    }();

    if (!item_path.is_empty()) {
      name = Path::name(item_path);

      APP_CALL_GRAPH_TRACE_MESSAGE(item_path);

      SL_PRINTER_TRACE("loading appfs info for " + item_path);
      api::ErrorScope error_scope;
      const auto info = Appfs(link_path.driver()).get_info(item_path);
      if (is_success()) {
        Printer::Object po(printer().active_printer(), name);
        session_settings().tag_list().push_back(String("project:" + info.id()));
        printer() << info;
      } else {
        if (!file_info.is_directory()) {
          Printer::Object po(printer().active_printer(), name);
          printer().info("not executable");
          printer().object("error", error(), Printer::Level::trace);
        } else {
          SL_PRINTER_TRACE(
            "application information for `" + item_path + "` not valid");
        }
      }
    }
  }

  return is_success();
}

bool Application::profile(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      profile,
      "creates testing sub-projects and manages code coverage using `gcov`.")
      + GROUP_ARG_REQ(
        path_p,
        string,
        <path>,
        "Path to the application to profile")
      + GROUP_ARG_OPT(
        build_target,
        string,
        release,
        "is usually set to`release` or `debug`")
      + GROUP_ARG_OPT(
        configure,
        bool,
        true,
        "Configure the test suite using the `sl_test_settings.json` file "
        "(`false` if `report` is `true`)")
      + GROUP_ARG_OPT(
        compile,
        bool,
        true,
        "Build the test applications (`false` if `report` is `true`)")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "Just show what actions would be performed.")
      + GROUP_ARG_OPT(
        run,
        bool,
        true,
        "Run the test applications (`false` if `report` is `true`)")
      + GROUP_ARG_OPT(
        name,
        string,
        <all>,
        "Operate only on the specified test.")
      + GROUP_ARG_OPT(
        synchronize_sync,
        bool,
        true,
        "Synchronize the test data on the device to the source tree and run "
        "`gcov` to create intermediate coverage files.")
      + GROUP_ARG_OPT(
        report,
        bool,
        false,
        "Parse the `gcov` output into a test report (`false` if `configure`, "
        "`compile`, or `run` is `true`).")
      + GROUP_ARG_OPT(
        datapath,
        string,
        <default>,
        "clean out test coverage files."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView configure = command.get_argument_value("configure");
  StringView compile = command.get_argument_value("compile");
  StringView run = command.get_argument_value("run");
  StringView data_path = command.get_argument_value("datapath");
  StringView report = command.get_argument_value("report");

  if (report == "true") {
    compile = "false";
    run = "false";
    configure = "false";
  }

  if (data_path.is_empty()) {
    data_path = "device@/home";
  }

  APP_RETURN_ASSIGN_ERROR(
    "This operation is not supported or functional right now");

}

bool Application::configure_test(
  const var::String &path,
  const TestSettings &test_settings,
  const TestDetails &test_details) {

  SL_PRINTER_TRACE("configuring test " + test_details.key());

  Printer::Object po(printer().output(), test_details.key());

  // create test project directory if it doesn't exist
  PathString project_directory_path
    = test_settings.get_project_directory_path(path, test_details);

  printer().key("projectDirectory", project_directory_path);

  if (!FileSystem().directory_exists(project_directory_path)) {

    FileSystem().create_directory(
      project_directory_path,
      Dir::IsRecursive::yes,
      Permissions(0777));
  }

  // populate test project directory with CMakeLists.txt and sl_settings.json
  printer().key("cmake", "CMakeLists.txt");
  {
    File f(
      File::IsOverwrite::yes,
      (PathString(project_directory_path) / "CMakeLists.txt").string_view());

    f.write("cmake_minimum_required (VERSION 3.6)\n")
      .write("\n")
      .write("include(../../test.cmake)\n")
      .write("\n")
      .write(String("set(SOS_NAME " + test_details.key() + ")\n"))
      .write("project(${SOS_NAME} CXX C)\n")
      .write("\n")
      .write("set_source_files_properties(\n");

    for (const auto &source : test_details.get_source_list()) {
      f.write("\t${CMAKE_CURRENT_SOURCE_DIR}/../../../" + source + "\n");
    }
    f.write("\tPROPERTIES COMPILE_FLAGS \"-fprofile-arcs -ftest-coverage\"\n")
      .write("\t)\n")
      .write(
        String("set(SOS_DEFINITIONS -D__TEST=1 -DTEST_")
        + KeyString(test_details.key()).to_upper() + "=1)\n")
      .write("include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-app.cmake)\n")
      .write("\n");
  }

  printer().key("sl", "sl_settings.json");
  {
    Project()
      .set_name(test_details.key())
      .set_version("0.1")
      .set_type("app")
      .set_description("test coverage application")
      .set_permissions("private")
      .export_file(
        File(PathString(project_directory_path) / Project::file_name()));
  }

  return true;
}

/*
E67E82CDBB4DEBC3DAC24AF38EFA5585F01600E48528699B485CDB03D584260D
Mj3NMqEyRKzRIhiAX5V9
*/
