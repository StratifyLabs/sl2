// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <errno.h>
#include <chrono.hpp>
#include <fs.hpp>
#include <inet.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "Sdk.hpp"
#include "utilities/Packager.hpp"

class CompileCommand : public JsonValue {
public:
  CompileCommand() : JsonValue(JsonObject()) {}
  CompileCommand(const JsonObject &object) : JsonValue(object) {}
  JSON_ACCESS_STRING(CompileCommand, directory);
  JSON_ACCESS_STRING(CompileCommand, command);
  JSON_ACCESS_STRING(CompileCommand, file);
};

using CompileCommandList = Vector<CompileCommand>;

Sdk::Sdk() : Connector("sdk", "sdk") {}

var::StringList Sdk::get_command_list() const {

  StringViewList list = {"install", "update", "publish", "copyright", "export"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool Sdk::execute_command_at(u32 list_offset, const Command &command) {


  switch (list_offset) {
  case command_install:
    return install(command);
  case command_update:
    return update(command);
  case command_publish:
    return publish(command);
  case command_copyright:
    return copyright(command);
  case command_export:
    return export_command(command);
  }
  return false;
}

bool Sdk::copyright(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
				Command::Group(get_name()),
				GROUP_ARG_DESC(install,"inserts a copyright message on the first line of each file in a project.") +
				GROUP_ARG_OPT(dryrun,bool,false:,"just show which files need to be updated.") +
				GROUP_ARG_OPT(files,string,'c?h?cpp?hpp',"file suffixes to modify.") +
				GROUP_ARG_OPT(filter,string,cmake,"paths to ignore if the path contains a filter element.") +
				GROUP_ARG_REQ(message_msg,string,<message>,"exact string to insert.") +
				GROUP_ARG_REQ(path_p,string,<path>,"relative path to the application project folder on the host computer.") +
				GROUP_ARG_OPT(prefix,string,'//COPYING:',"prefix for each message.") +
				GROUP_ARG_OPT(replace,string,<none>:,"deprecated prefix that will be replaced.")
				);

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  String path = command.get_argument_value("path");
  String files = command.get_argument_value("files");
  String message = command.get_argument_value("message");
  String prefix = command.get_argument_value("prefix");
  String replace_prefix = command.get_argument_value("replace");
  String filter = command.get_argument_value("filter");
  String dryrun = command.get_argument_value("dryrun");

  command.print_options(printer());

  printer().open_output();
  files.replace(String::ToErase("'"), String::ToInsert(""));
  Vector<String> suffix_list = files.split("?");
  Vector<String> filter_list = filter.split("?");

  for (const auto &suffix : suffix_list) {
    SL_PRINTER_TRACE("suffix -> " + suffix);
  }

  for (const auto &filter_item : filter_list) {
    SL_PRINTER_TRACE("filter -> " + filter_item);
  }

  Vector<String> file_list;
  {
    Vector<String> preliminary_file_list
      = Dir::read_list(path, Dir::IsRecursive(true));
    for (const auto &file : preliminary_file_list) {

      bool is_filtered = false;
      for (const auto &filter_item : filter_list) {
        if (file.find(filter_item) != String::npos) {
          is_filtered = true;
          break;
        }
      }

      if (!is_filtered) {
        size_t entry_offset = suffix_list.find(FileInfo::suffix(file));
        if (entry_offset < suffix_list.count()) {
          file_list.push_back(path + "/" + file);
        }
      }
    }
  }

  String prefix_to_replace;
  if (replace_prefix.is_empty()) {
    prefix_to_replace = prefix;
  } else {
    prefix_to_replace = replace_prefix;
  }

  String copyright_line = prefix + " " + message + "\n";

  for (const auto &file : file_list) {
    SL_PRINTER_TRACE("process " + file);
    File f;

    if (f.open(file, OpenMode::read_only()) < 0) {
      printer().error("Failed to open " + file);
      return printer().close_fail();
    }

    DataFile file_copy(OpenMode::append_read_write());
    int result = file_copy.write(f);
    SL_PRINTER_TRACE(String().format("read %d bytes from file", result));
    f.close();

    file_copy.seek(0);

    // read the first line
    String first_line = file_copy.gets();

    SL_PRINTER_TRACE("first line of " + file + " is " + first_line);

    if (first_line != copyright_line) {
      // replace the file
      if (dryrun == "false") {
        if (f.create(file, File::IsOverwrite(true)) < 0) {
          printer().error("failed to modify " + file);
          return printer().close_fail();
        }

        f.write(copyright_line);
        if (first_line.find(prefix_to_replace) != 0) {
          // if the replace marker isn't found keep the original first
          // line
          SL_PRINTER_TRACE(
            "didn't find marker " + prefix_to_replace + " in " + first_line);
          f.write(first_line);
        }

        f.write(file_copy);
        f.close();
        printer().output_key(file, "updated");
      } else {
        printer().output_key(file, "outdated");
      }

    } else {
      SL_PRINTER_TRACE("copyright is already current");
      printer().output_key(file, "current");
    }
  }

  return printer().close_success();
}

bool Sdk::install(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
    Command::Group(get_name()),
    "install:description=opt_string|The `sdk.install` command will download "
    "and install the software development kit "
    "which includes a GCC compiler as well as pre-built libraries for Stratify "
    "OS.||sdk.install|,path_p=opt_string_"
#if defined __win32
    "C:/"
#elif defined __macosx
    "/Applications/"
#else
    "/"
#endif
    "StratifyLabs-SDK|destination for install.||sdk.install:path=<path>|"
      + GROUP_ARG_OPT(
        7z,
        bool,
        false,
        "install the compiler complete with the SDK (others are false if this "
        "is `true`.")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "download the package but don't extract it.")
      + GROUP_ARG_OPT(clean, bool, true, "delete temporary files.")
      + GROUP_ARG_OPT(
        version,
        bool,
        <latest>,
        "specify the compiler or SDK version to install."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  String clean = command.get_argument_value("clean");
  String dryrun = command.get_argument_value("dryrun");
  String version = command.get_argument_value("version");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  printer().open_output();

  bool result = true;

  result = Packager()
             .set_keep_temporary(clean != "true")
             .deploy(Packager::DeployOptions()
                       .set_extract(dryrun != "true")
                       .set_name("compiler")
                       .set_version(version)
                       .set_destination_directory_path(
                         OperatingSystem::get_sdk_directory_path()));

  if (result == false) {
    return printer().close_fail();
  }

  return printer().close_success();
}

bool Sdk::download_and_execute(
  const var::String &url,
  const var::String &command,
  const var::String &archive_name) {

  SecureSocket socket;
  HttpClient http_client(socket);
  File archive_file;

  SL_PRINTER_TRACE("create file " + archive_name);
  if (archive_file.create(archive_name, fs::File::IsOverwrite(true)) < 0) {
    printer().error("Failed to create archive file for download");
    return false;
  }

  SL_PRINTER_TRACE("download file " + archive_name);
  if (
    http_client.get(
      Http::UrlEncodedString(url),
      Http::ResponseFile(archive_file),
      printer().progress_callback())
    < 0) {
    printer().error("Failed to retrieve tools");
    archive_file.close();
    File::remove(archive_name);
    return false;
  }

  SL_PRINTER_TRACE("close file");
  archive_file.close();

  SL_PRINTER_TRACE("extracting downloaded file");
  if (
    execute_system_command(ExecuteSystemCommandOptions().set_command(command))
    != 0) {
    printer().error("Failed to extract archive");
    File::remove(archive_name);
    return false;
  }

  SL_PRINTER_TRACE("remove file " + archive_name);
  File::remove(archive_name);

  return true;
}

bool Sdk::update(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      update,
      "uses git to pull libraries from remote git repositories then build and "
      "install the libraries on the local "
      "machine. If no library (or libraries) are specified, the default "
      "Stratify libraries will be pulled or if "
      "libraries exist in the workspace, they will be pulled/build/installed.")
      + GROUP_ARG_OPT(build_target, string, install, "cmake build target.")
      + GROUP_ARG_OPT(
        reconfigure,
        bool,
        false,
        "delete the build folders and re-run cmake.")
      + GROUP_ARG_OPT(clean, bool, false, "clean before building code.")
      + GROUP_ARG_OPT(compile, bool, true, "compile the code (using `make`.")
      + GROUP_ARG_OPT(
        configure,
        bool,
        true,
        "configure the code (using `cmake`).")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "list actions without performing them (set to true if library is "
        "provided).")
      + GROUP_ARG_OPT(
        generator_g,
        string,
        <default>,
        "CMake Generator type (if not specified, it is chosen based on the "
        "environment).")
      + GROUP_ARG_OPT(
        install_i,
        bool,
        true,
        "install the SDK on the local machine at the default path "
        "(C:/StratifyLabs-SDK or "
        "/Applications/StratifyLabs-SDK).")
      + GROUP_ARG_OPT(
        pull,
        bool,
        true,
        "pull the latest code from Github (can be false on subsequent calls to "
        "*sdk.update*)")
      + GROUP_ARG_OPT(
        remove,
        bool,
        false,
        "delete all SDK project folders in the current workspace (useful if a "
        "'tag' is changed so it can be re-cloned "
        "and checked out).")
      + GROUP_ARG_OPT(
        status,
        bool,
        false,
        "show the git status of each repository in the SDK (all other actions "
        "will be false be default)"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  String generator = command.get_argument_value("generator");
  String build_target = command.get_argument_value("build");
  String pull = command.get_argument_value("pull");
  String compile = command.get_argument_value("compile");
  String reconfigure = command.get_argument_value("reconfigure");
  String configure = command.get_argument_value("configure");
  String clean = command.get_argument_value("clean");
  String install = command.get_argument_value("install");
  String dryrun = command.get_argument_value("dryrun");
  String remove = command.get_argument_value("remove");
  String status = command.get_argument_value("status");

  if (build_target.is_empty()) {
    if (install == "true") {
      build_target = "install";
    } else {
      build_target = "all";
    }
  }

  if (status == "true") {
    pull = compile = clean = install = remove = "false";
    dryrun = "true";
  }

  command.print_options(printer());

  printer().open_output();

  m_project_list = workspace_settings().get_sdk();
  String sdk_directory = workspace_settings().get_sdk_directory();

  if (Dir::exists(sdk_directory) == false) {
    if (Dir::create(sdk_directory, Permissions(0777)) < 0) {
      printer().error("Failed to create SDK directory");
      return printer().close_fail();
    }
  }

  // get list from workspace settings
  SL_PRINTER_TRACE("Check for existing workspace libraries");

  if (m_project_list.count() == 0) {
    printer().error("no libraries have been configured");
    printer().troubleshoot(
      "This workspace has not been initialized with any SDK build definitions. "
      "You can initialize with the default "
      "libraries using `sl sdk.configure`. This will configure the workspace "
      "to build Stratify OS, the Stratify API "
      "library plus a few other dependent libraries.");
    return printer().close_fail();
  }

  printer().open_object("tasks", Printer::Level::info);
  SL_PRINTER_TRACE(
    String().format("preparing to build %d projects", m_project_list.count()));
  for (SdkProject &project : m_project_list) {

    String sdk_project_path = sdk_directory + "/" + project.get_name();

    printer().open_object(project.get_name(), Printer::Level::info);

    if (remove == "true") {
      printer().key("remove", sdk_project_path);
      if (dryrun != "true") {
        if (Dir::remove(sdk_project_path, Dir::IsRecursive(true)) < 0) {
          printer().warning(
            "failed to remove directory '%s'",
            sdk_project_path.cstring());
        }
      }
    }

    if (pull == "true" && !project.get_url().is_empty()) {

      bool directory_exists = is_directory_valid(sdk_project_path, true);

      if (project.get_tag() == "HEAD" || !directory_exists) {

        String full_url;
        if (project.get_url() == "github.com/StratifyLabs") {
          full_url = String() << "https://" << project.get_url() << "/"
                              << project.get_name() << ".git";
        } else {
          full_url = project.get_url();
        }

        SL_PRINTER_TRACE(String().format(
          "clone project %s from %s",
          project.get_name().cstring(),
          full_url.cstring()));
        SL_PRINTER_TRACE("");

        if (directory_exists) {
          printer().key("pull", sdk_project_path);
        } else {
          printer().key("clone", full_url);
        }

        if (dryrun != "true") {
          if (clone_project(sdk_project_path, full_url) == false) {
            printer().close_object(); // project
            printer().close_object(); // tasks
            return printer().close_fail();
          }
        }

        if (directory_exists == false) {
          String checkout_name;
          if (project.get_tag().is_empty() == false) {
            checkout_name = project.get_tag();
            if (project.get_branch() != "master") {
              printer().warning("'tag' takes priority over 'branch'");
            }
          } else {
            checkout_name << "-b " << project.get_branch();
          }
          printer().key("checkout", checkout_name);
          if (dryrun != "true") {
            String execute_command
              = "git -C " + sdk_project_path + " checkout " + checkout_name;
            if (
              execute_system_command(
                ExecuteSystemCommandOptions().set_command(execute_command))
              != 0) {
              printer().error(
                "failed to checkout branch/tag command '%s'",
                project.get_tag().cstring());
              printer().close_object(); // project
              printer().close_object(); // tasks
              return printer().close_fail();
            }
          }
        }
      }
    }

    if (!project.get_command().is_empty()) {
      printer().key("command", project.get_command());
      if (dryrun != "true") {
        if (
          execute_system_command(
            ExecuteSystemCommandOptions().set_command(project.get_command()))
          != 0) {
          printer().error(
            "failed to execute prebuild command '%s'",
            project.get_command().cstring());
          printer().close_object(); // project
          printer().close_object(); // tasks
          return printer().close_fail();
        }
      }
    }

    if (!project.get_pre_build().is_empty()) {
      printer().key("prebuild", project.get_pre_build());
      if (dryrun != "true") {
        if (
          execute_system_command(
            ExecuteSystemCommandOptions().set_command(project.get_pre_build()))
          != 0) {
          printer().error(
            "failed to execute prebuild '%s'",
            project.get_pre_build().cstring());
          printer().close_object(); // project
          printer().close_object(); // tasks
          return printer().close_fail();
        }
      }
    }

    if (status == "true") {
      PrinterObject po(printer().output(), project.get_name() + " status");
      execute_system_command(ExecuteSystemCommandOptions().set_command(
        "cmake -E chdir " + project.get_name() + " git status"));
    }

    if (
      ((compile == "true") || (configure == "true"))
      && project.is_buildable()) {
      Tokenizer architecture_list(
        project.get_architecture(),
        Tokenizer::Delimeters("?"));
      for (u32 j = 0; j < architecture_list.count(); j++) {
        String architecture = architecture_list.at(j);
        String cmake_options;

        String additional_cmake_options;
        // add SOS_SDK_PATH to cmake options if it isn't there
        if (
          project.get_cmake_options().find("SOS_SDK_PATH") == String::npos
          && (getenv("SOS_SDK_PATH") == nullptr)) {
          additional_cmake_options = "-DSOS_SDK_PATH=\""
                                     + OperatingSystem::get_sdk_directory_path()
                                     + "\"";
        }

        if (session_settings().is_sdk_invoke_mismatch()) {
          printer().troubleshoot(
            "You can use `sl --version` to see the SDK associated with the "
            "`sl` invocation. Use `echo $SOS_SDK_PATH` to "
            "see the environment variable. The values must match");
          printer().error(
            "`sl` invoked from a different location than environment variable "
            "SOS_SDK_PATH");
          return printer().close_fail();
        }

        if (
          project.get_cmake_options().is_empty() == false
          && additional_cmake_options.is_empty() == false) {
          cmake_options
            = project.get_cmake_options() + " " + additional_cmake_options;
        } else if (project.get_cmake_options().is_empty() == false) {
          cmake_options = project.get_cmake_options();
        } else {
          cmake_options = additional_cmake_options;
        }

        {
          PrinterObject po(printer().output(), "compile");
          printer().key("arch", architecture);
          printer().key("cmake options", cmake_options);
          printer().key("make options", project.get_make_options());
          printer().key("clean", clean);
          printer().key(
            "generator",
            generator.is_empty() ? String("<default>") : generator);
          printer().key("build", build_target);
          printer().key("reconfigure", reconfigure == "true");
        }

        if (dryrun != "true") {
          if (
            build_project(Project::BuildOptions()
                            .set_path(sdk_project_path)
                            .set_make_options(project.get_make_options())
                            .set_target(build_target)
                            .set_architecture(architecture)
                            .set_generator(generator)
                            .set_clean(clean)
                            .set_reconfigure(reconfigure == "true")
                            .set_cmake_options(cmake_options))
            == false) {
            printer().close_object(); // project
            printer().close_object(); // tasks
            return printer().close_fail();
          }
        }
      }
    }

    if (!project.get_post_build().is_empty()) {
      printer().key("postbuild", project.get_post_build());
      if (dryrun != "true") {
        if (
          execute_system_command(
            ExecuteSystemCommandOptions().set_command(project.get_post_build()))
          != 0) {
          printer().error(
            "failed to execute prebuild '%s'",
            project.get_post_build().cstring());
          printer().close_object(); // project
          printer().close_object(); // tasks
          return printer().close_fail();
        }
      }
    }

    printer().close_object(); // close project
  }
  printer().close_object(); // tasks

  return printer().close_success();
}

bool Sdk::publish(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      publish,
      "copies the SDK files (compiler as well as associated files) to a "
      "temporary directory and compress the files into a self-extracting "
      "archive. 7z must be available on the system `PATH` variable for the "
      "archive to work.")
      + GROUP_ARG_OPT(archive, bool, true, "create an archive of the SDK.")
      + GROUP_ARG_OPT(
        filter_filt,
        string,
        <none>,
        "filter to exclude matching entries (separate filters with ?).")
      + GROUP_ARG_OPT(clean, bool, true, "delete temporary files.")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "do all the gathering but don't publish."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  String filter = command.get_argument_value("filter");
  String compiler = command.get_argument_value("compiler");
  String sdk = command.get_argument_value("sdk");
  String clean = command.get_argument_value("clean");
  String dryrun = command.get_argument_value("dryrun");

  if (filter.is_empty()) {
    filter = "share/doc?/bin/sl";
    if (sys::System::is_windows()) {
      filter += "?/mingw";
    }
  }

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  printer().open_output();

  if (cloud().credentials().get_uid() != "9cQyOTk3fZXOuMtPyf5j9X4H4b83") {
    printer().info("forcing dryrun because you don't have publish permissions");
    dryrun = "true";
  }

  bool result = true;
  result
    = Packager()
        .set_keep_temporary(clean != "true")
        .publish(Packager::PublishOptions()
                   .set_name("compiler")
                   .set_dryrun(dryrun == "true")
                   .set_archive_name("Tools")
                   .set_filter(filter)
                   .set_source_path(OperatingSystem::get_sdk_directory_path()));

  if (result == false) {
    return printer().close_fail();
  }

  return printer().close_success();
}

bool Sdk::export_command(const Command &command) {
  printer().open_command("sdk.export");


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      export,
      "exports the compilation commands and source lists so that they can be "
      "built using a system other than cmake.")
      + GROUP_ARG_OPT(
        android,
        bool,
        false,
        "generate makefiles that can be easily used in an Android makefile "
        "project.")
      + GROUP_ARG_OPT(
        architecture_arch,
        string,
        <default>,
        "architecture to build (use SDK values by default).")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <current directory>,
        "location of the output files."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  String android = command.get_argument_value("android");
  String destination = command.get_argument_value("destination");
  String architecture = command.get_argument_value("architecture");

  command.print_options(printer());

  printer().open_output();

  m_project_list = workspace_settings().get_sdk();
  String sdk_directory = workspace_settings().get_sdk_directory();

  if (Dir::exists(sdk_directory) == false) {
    printer().error("SDK directory does not exist");
    printer().troubleshoot(
      "Please run `sl sdk.update` before using the `sdk.export` command.");
    return printer().close_fail();
  }

  // get list from workspace settings
  SL_PRINTER_TRACE("Check for existing workspace libraries");

  if (m_project_list.count() == 0) {
    printer().error("no libraries have been configured");
    return printer().close_fail();
  }

  for (SdkProject &project : m_project_list) {
    String sdk_project_path = sdk_directory + "/" + project.get_name();
    String architecture_string
      = architecture.is_empty() ? project.get_architecture() : architecture;

    if (project.is_buildable()) {
      Tokenizer architecture_list(
        architecture_string,
        Tokenizer::Delimeters("?"));
      SL_PRINTER_TRACE("architectures are " + project.get_architecture());

      String project_cmake_options
        = project.get_cmake_options() + " -DCMAKE_EXPORT_COMPILE_COMMANDS=ON";
      for (const String &architecture : architecture_list.list()) {

        if (project.get_architecture().find(architecture) == String::npos) {
          continue;
        }

        // run cmake
        String cmake_directory_path
          = sdk_project_path + "/cmake_" + architecture;
        String cmake_command;
        cmake_command = "cmake -E chdir " + cmake_directory_path + " cmake "
                        + project_cmake_options + " ..";

        SL_PRINTER_TRACE("executing `" + cmake_command + "`");
        execute_system_command(
          ExecuteSystemCommandOptions().set_command(cmake_command));

        CompileCommandList compile_command_list
          = JsonDocument()
              .load(JsonDocument::FilePath(
                cmake_directory_path + "/compile_commands.json"))
              .to_array()
              .construct_list<CompileCommand>();

        SL_PRINTER_TRACE(
          "build has " + NumberString(compile_command_list.count())
          + " commands");

        printer().open_object(project.get_name(), Printer::Level::info);
        printer().key("arch", architecture);
        printer().key("cmake options", project_cmake_options);
        printer().close_object(); // close project
        if (android == "true") {
          export_android_makefiles(AndroidOptions()
                                     .set_destination(destination)
                                     .set_name(project.get_name())
                                     .set_source(cmake_directory_path));
        }
      }
    }
  }

  return printer().close_success();
}

bool Sdk::get_directory_entries(
  var::Vector<var::String> &list,
  const var::String &path) {
  var::Vector<var::String> dir_list = Dir::read_list(path);

  for (u32 i = 0; i < dir_list.count(); i++) {
    String entry_path;
    String entry = dir_list.at(i);
    entry_path << path << "/" << entry;

    Stat info(true);
    info = File::get_info(entry_path);
    if (info.is_valid() == false) {
      printer().warning("failed to get info for %s", entry_path.cstring());
    } else {
      if (
        info.is_directory() && entry != "." && entry != ".."
        && entry != "__MACOSX") {
        if (get_directory_entries(list, entry_path) == false) {
          return false;
        }
      } else if (info.is_file() && entry != ".DS_Store") {
        list.push_back(entry_path);
      }
    }
  }

  return true;
}

Vector<SdkProject>
Sdk::bootstrap_sdk_workspace(const var::String &make_options) {
  Vector<SdkProject> result;


  String effective_make_options = make_options;
  if (effective_make_options.is_empty()) {
    effective_make_options = "-j8";
  }

  result.push_back(
    SdkProject()
      .set_name("StratifyOS")
      .set_url("https://github.com/StratifyLabs/StratifyOS.git")
      .set_cmake_options(
        "-DSOS_SKIP_CMAKE=OFF -DBUILD_ALL=ON -DSOS_ARCH_ARM_ALL=ON"));

  printer() << result.back().to_object();

  result.push_back(SdkProject()
                     .set_name("sgfx")
                     .set_url("https://github.com/StratifyLabs/sgfx.git")
                     .set_make_options(effective_make_options));

  result.push_back(
    SdkProject()
      .set_name("StratifyOS-CMSIS")
      .set_url("https://github.com/StratifyLabs/StratifyOS-CMSIS.git")
      .set_make_options(effective_make_options)
      .set_command("cmake -E chdir StratifyOS-CMSIS cmake -P bootstrap.cmake"));

  result.push_back(
    SdkProject()
      .set_name("StratifyOS-mbedtls")
      .set_url("https://github.com/StratifyLabs/StratifyOS-mbedtls.git")
      .set_make_options(effective_make_options)
      .set_command(
        "cmake -E chdir StratifyOS-mbedtls cmake -P bootstrap.cmake"));

  result.push_back(
    SdkProject()
      .set_name("StratifyOS-jansson")
      .set_url("https://github.com/StratifyLabs/StratifyOS-jansson.git")
      .set_make_options(effective_make_options)
      .set_command(
        "cmake -E chdir StratifyOS-jansson cmake -P bootstrap.cmake"));

  result.push_back(SdkProject()
                     .set_name("StratifyAPI")
                     .set_url("https://github.com/StratifyLabs/StratifyAPI.git")
                     .set_make_options(effective_make_options));

  return result;
}

bool Sdk::export_android_makefiles(const AndroidOptions &options) const {

  CompileCommandList compile_command_list
    = JsonDocument()
        .load(
          JsonDocument::FilePath(options.source() + "/compile_commands.json"))
        .to_array()
        .construct_list<CompileCommand>();

  if (compile_command_list.is_empty()) {
    printer().warning("no compile commands");
    return false;
  }

  if (Dir::exists(options.destination()) == false) {
    if (Dir::create(options.destination()) < 0) {
      printer().error(
        "failed to create destination directory " + options.destination());
      return false;
    }
  }

  File mk_file;
  const String mk_file_path
    = options.destination() + "/" + options.name() + ".mk";

  mk_file.create(mk_file_path, File::IsOverwrite(true));

  if (mk_file.return_value() < 0) {
    printer().error("failed to create " + mk_file_path);
    return false;
  }

  mk_file.write(
    "SRC_DIRECTORY := " + FileInfo::parent_directory(options.source())
    + "\n\n");

  mk_file.write(String("SRC_FILES := \\\n"));
  for (const CompileCommand &compile_command : compile_command_list) {
    mk_file.write("\t" + compile_command.get_file() + " \\\n");
  }

  mk_file.write("\n\n\n");
  return true;
}
