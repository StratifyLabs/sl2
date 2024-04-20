#include <chrono.hpp>
#include <printer.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "Group.hpp"
#include "Switch.hpp"
#include "utilities/Shortcut.hpp"

var::Vector<Switch> Switch::get_list() {
  return Vector<Switch>()
    .push_back(Switch(
      "help",
      "shows help information. Usage: `sl --help[=<group>][.<command>]`"))
    .push_back(Switch(
      "offline",
      "operate in offline mode (not all features are available)"))
    .push_back(Switch("listen", "run as an HTTP server using JSON requests"))
    .push_back(Switch("webpath", "path to an HTTP site to serve"))
    .push_back(Switch(
      "update",
      "checks for updates to `sl` and downloads the latest version without "
      "making it effective. To make the latest "
      "version effective, you need to execute `slu` to replace the "
      "downloaded "
      "version with the new version. Usage: `sl "
      "--update`"))
    .push_back(Switch(
      "debug",
      "shows low level debugging. Example: `sl os.ping --debug=10`"))
    .push_back(Switch(
      "shortcuts",
      "shows the shortcuts available in the workspace. Example: `sl "
      "--shortcuts`"))
    .push_back(Switch(
      "version",
      "shows version information for `sl`. Example: `sl --version`"))
    .push_back(Switch(
      "initialize",
      "Initializes the current directory as an `sl` workspace. Example: `sl "
      "--initialize`"))
    .push_back(Switch(
      "shortcut",
      "saves the commands as a shortcut. Example: `sl term.run "
      "--save=runTerminal`"))
    .push_back(Switch(
      "readme",
      "prints the entire readme document. Example: `sl --readme`"))
    .push_back(Switch(
      "verbose",
      "sets the verbose level. Usage: `sl "
      "--verbose=<fatal|error|warning|info|message|debug>`"))
    .push_back(Switch(
      "vanilla",
      "disables color and other formatting options in the output. Example: "
      "`sl "
      "debug.trace --vanilla`"))
    .push_back(Switch(
      "json",
      "prints output in JSON format. Example: `sl --version --json`"))
    .push_back(Switch(
      "archivehistory",
      "archives the workspace history. The history is automatically archived "
      "from time to time. Example: `sl "
      "--archivehistory`"))
    .push_back(Switch("codefences", "add code fences to structured output"))
    .push_back(Switch(
      "report",
      "save a report of the program output to your cloud account (implies "
      "`--vanilla`"))
    .push_back(Switch("changes", "show a list of the changes to `sl`"))
    .push_back(
      Switch("graph", "create a call graph of the execution of the sl program"))
    .push_back(Switch(
      "copy",
      "make a copy of sl called slc (useful for rebuilding and installing "
      "from "
      "source)"));
}

bool Switch::handle_switches(const sys::Cli &cli) {
  APP_CALL_GRAPH_TRACE_FUNCTION();

  /* Switches
   *
   * --silent - no output except terminal, task, debug
   * --help - show help groups
   * --help=group - show help for group
   * --debug=level
   *
   */

  // first check for bad switches
  SL_PRINTER_TRACE("looping through switches");
  const auto switch_list = Switch::get_list();

  const auto shortcut_list = Shortcut::list();

  auto workspace_switch_list = []() {
    api::ErrorContext ec;
    return Group::workspace_settings()
      .to_object()
      .at("switches")
      .to_object()
      .get_key_list();
  }();

  for (auto i : api::Index(cli.count())) {
    if (cli.at(static_cast<u16>(i)).find("-") == 0) {
      auto switch_value = cli.at(i).split("=");
      StringView switch_name = switch_value.at(0);
      if (switch_name.find(("--")) == 0) {
        switch_name = switch_name.get_substring_at_position(2);
      } else {
        switch_name = switch_name.get_substring_at_position(1);
      }

      const auto is_switch_found = [&]() {
        const auto is_switch_found_in_switches = switch_list.any_of(
          [&](const Switch &sw) { return sw.name() == switch_name; });
        return is_switch_found_in_switches
               || workspace_switch_list.any_of(
                 [&](StringView key) { return key == switch_name; });
      }();

      if (
        !is_switch_found
        && shortcut_list.find_offset(ShortcutInfo(switch_name, JsonObject()))
             == shortcut_list.count()) {
        printer().troubleshoot(
          "Use `sl --help` for a list of switches. Use `sl --shortcuts` "
          "for a "
          "list of workspace shortcuts.");
        printer().syntax_error(
          "`" + switch_name + "` is not a recognized switch or shortcut");
        return false;
      }
    }
  }

  StringView value;
  value = cli.get_option("vanilla");
  if (value == "true") {
    printer().set_vanilla();
  } else if (!value.is_empty()) {
    printer().syntax_error("`--vanilla` does not take arguments");
  }

  value = cli.get_option("copy");
  if (!value.is_empty()) {
    // create a copy called slc
    printer().open_command("sl.copy");
    const auto loc = value != "true" ? PathString(value)
                                     : workspace_settings().sl_location();
    SlPrinter::Output printer_output_guard(printer());
    if (!(App::create_sl_copy(value == "true" ? "" : value))) {
      printer().error("failed to make a copy of `sl` as `slc`");
    } else {
      printer().key("slc", loc);
      printer().close_success();
    }
  }

  value = cli.get_option("json");
  if (value == "true") {
    printer().set_json();
  } else if (!value.is_empty()) {
    printer().set_json();
    printer().syntax_error("`--json` does not take arguments");
  }

  value = cli.get_option("codefences");
  if (value == "true") {
    printer().set_insert_codefences();
  } else if (!value.is_empty()) {
    printer().syntax_error("`--codefences` does not take arguments");
  }

  // verbose needs to be first so it is effective when other switches are
  // executed
  value = cli.get_option(("verbose"));
  if (value == "true") {
    printer().troubleshoot(
      "use `use 'sl settings.set:add,key=verbose,value=<level>' to set the "
      "default`");
    printer().syntax_error(
      "`--verbose` takes values of debug|info|message|warning|error|fatal");
  } else if (value != "false") {
    printer().set_verbose_level(value);
  }

  value = cli.get_option("offline");
  session_settings().set_offline_mode(value == "true");

  value = cli.get_option("initialize");
  if (value == "true") {
    if (!workspace_settings().is_valid()) {
      printer().open_command("workspace");
      SlPrinter::Output printer_output_guard(printer());
      if (workspace_settings().initialize_workspace() < 0) {
        printer().error(
          "Failed to initialize current directory as an `sl` workspace.");
        printer().close_fail();
      } else {
        printer().info(
          "Created `" + FilePathSettings::workspace_settings_filename()
          + "` in the current directory.");
        printer().close_success();
      }
      printer().close_command();
    }
  }

  SL_PRINTER_TRACE("checking for readme");
  value = cli.get_option("readme");
  if (!value.is_empty()) {
    show_readme(value == "true" ? "" : value);
  }

  SL_PRINTER_TRACE("checking for changes");
  value = cli.get_option("changes");
  if (value == "true") {
    show_changes();
  }

  SL_PRINTER_TRACE("checking for help");
  value = cli.get_option("help");
  if (value == "true" || (cli.count() == 1)) {

    MarkdownPrinter::Header hg(printer().markdown(), "help");
    {
      printer().insert_newline();
      printer().open_command("switches");
      {
        MarkdownPrinter::Paragraph pg(printer().markdown());
        printer() << "Usage is `sl --<option>[=<value>]`";
        printer().open_options(Printer::Level::info);
        for (const auto &s : switch_list) {
          printer().key(s.name(), s.description());
        }
        printer().key(
          "<shortcut>[=args...]",
          String("execute the specified shortcut. `args` are `,` separated and "
                 "match `?1`, `?2` ... in the shortcut definition"));
        printer().close_options();
      }
      printer().close_command();
      printer().insert_newline();
      printer().open_command("`group.command` help syntax");
      {
        {
          MarkdownPrinter::Paragraph pg(printer().markdown());
          printer() << "Use `sl --help=<group>` for a list of commands for the "
                       "specified group.";
          printer() << "Use `sl --help=<group>.<command>` for help with a "
                       "specific command.";
          printer() << "All groups have a short version and a long version "
                       "listed below as <short>: <long>.";
        }

        MarkdownPrinter::Header hg(printer().markdown(), "groups");
        for (u32 i = 0; i < Group::group_list().count(); i++) {
          printer().key(
            Group::group_list().at(i)->get_shortcut(),
            String(Group::group_list().at(i)->get_name()));
        }
        printer().close_command();
      }
    }
  } else if (!value.is_empty()) {
    auto help_command = value.split(".");

    const StringView group = help_command.at(0);
    SL_PRINTER_TRACE("help for group " + group);

    if (help_command.count() == 1) {
      SL_PRINTER_TRACE("group level help");
      bool is_group_found = false;
      for (u32 i = 0; i < Group::group_list().count(); i++) {
        if (
          (group == Group::group_list().at(i)->get_name())
          || (group == Group::group_list().at(i)->get_shortcut())) {
          is_group_found = true;
          printer().open_header("`" + group + ".<command>` help");

          printer() << String(
                         "For help on a spefic command, use `sl --help=" + group
                         + ".<command>`.")
                         .string_view();

          printer().insert_newline();

          printer().open_command("commands");
          printer().open_list(printer::MarkdownPrinter::ListType::unordered);
          for (const StringView command :
               Group::group_list().at(i)->get_command_list()) {
            printer().key(StringView().set_null(), command);
          }
          printer().close_list();
          printer().close_command();
        }
      }
      if (!is_group_found) {
        printer().error("`" | group | "` is not a recognized group");
        printer().troubleshoot("use `sl --help` for a list of command groups");
      }
    } else {
      // sl --help=application.install
      StringView command = help_command.at(1);
      SL_PRINTER_TRACE("command level help " + command);

      printer().open_command("help `" + group + "." + command + "`");
      printer().set_suppressed(true);
      for (u32 i = 0; i < Group::group_list().count(); i++) {
        StringView local_group = help_command.at(0);
        if (
          (local_group == Group::group_list().at(i)->m_name)
          || (local_group == Group::group_list().at(i)->get_shortcut())) {
          SL_PRINTER_TRACE("execute help command " + command + ":help");
          Group::group_list().at(i)->execute_command(command + ":help");
        }
      }
      printer().set_suppressed(false);
      printer().close_command();
    }
  }

  value = cli.get_option(("debug"));
  if (value == "true") {
    printer().key("usage", String("--debug=<level>"));
  } else {
    link_set_debug(value.to_integer());
  }

  value = cli.get_option("report");
  if (!value.is_empty()) {

    if (
      value == "true" || value == "dryrun"
      || Document::is_permissions_valid(value)) {
      printer().set_insert_codefences();

      if (value != "true" && value != "dryrun") {
        session_settings().set_report_permissions(value);
        SL_PRINTER_TRACE(
          "Create a report with permissions: "
          + session_settings().report_permissions());
      } else if (value == "true") {
        SL_PRINTER_TRACE("Create a report with default permissions");
      } else {
        SL_PRINTER_TRACE("Print a report only -- dryrun");
      }
    } else {
      printer().syntax_error("use `--report[=<true|dryrun|public|private>]`");
      return false;
    }
  }

  value = cli.get_option("job");
  if (value) {
    if (value == "true" || value == "false") {
    }
  }

  value = cli.get_option("graph");
  if (value) {
    if (value != "false") {
      const auto file_path
        = (value == "true")
            ? PathString("sl_callgraph_report")
                .append(ClockTime::get_system_time().to_unique_string())
                .append(".md")
                .string_view()
            : value;

      App::session_settings().set_call_graph_path(file_path);

    } else {
      printer().syntax_error("use `--graph[=<true|dryrun|public|private>]`");
      return false;
    }
  }

  value = cli.get_option("update");
  if (value) {
    session_settings().set_request_update();
    App::execute_update(value);
  }

  value = cli.get_option(("listen"));
  if (value) {
    if (value != "true") {
      session_settings().set_listen_port(value.to_integer());
      if (session_settings().listen_port() == 0) {
        session_settings().set_listen_port(3000);
      }
    }
    session_settings().set_listen_mode();
  }

  value = cli.get_option(("webpath"));
  if (value) {
    if (value != "true") {
      session_settings().set_web_path(value);
    }
  }

  value = cli.get_option("version");
  if (value) {
    if (value != "true") {
      printer().warning("ignoring `" + value + "` use `sl --version`");
    }

    printer().open_command("about");
    SlPrinter::Output printer_output_guard(printer());
    printer().output_key("publisher", "Stratify Labs, Inc");
    printer().output_key("version", VERSION);
    printer().output_key("gitHash", SOS_GIT_HASH);
    printer().output_key("apiVersion", api::ApiInfo::version());
    printer().output_key("apiGitHash", api::ApiInfo::git_hash());
    printer().output_key("system", OperatingSystem::system_name());
    printer().output_key("sl", OperatingSystem::get_path_to_sl());
    if (is_admin()) {
      printer().output_key("admin", "true");
    }
    printer().close_output("result", "success");
    printer().close_command();
  }

  return handle_shortcuts(cli);
}

bool Switch::handle_shortcuts(const sys::Cli &cli) {
  bool result = true;

  const auto shortcut_list = Shortcut::list();
  auto value = cli.get_option("shortcut");
  if (value == "true") {
    printer().key("usage", String("--shortcut=<name>"));
    result = false;
  } else if (value) {
    // save this as a shortcut

    // pull out the command with no switches
    String command;
    command.reserve(1024);
    for (u16 i = 1; i < cli.count(); i++) {
      if (cli.at(i).find(("--")) != 0) {

        command += (cli.at(i).to_string() + " ");
      }
    }
    SL_PRINTER_TRACE("shortcut.run: " + value + " -> " + command);
    result = Shortcut::save(value, command);
    if (!result) {
      printer().syntax_error(
        "`" + value + "` cannot be used as a shortcut name");
    }
  }

  // list shortcuts
  value = cli.get_option("shortcuts");
  if (value) {
    printer().open_command("shortcuts");
    SlPrinter::Output printer_output_guard(printer());

    if (value != "true") {
      printer().syntax_error("Usage is `sl --shortcuts`");
    } else {

      if (shortcut_list.count() > 0) {
        for (const auto &shortcut : shortcut_list) {
          printer().key(shortcut.key(), shortcut.value().to_string_view());
        }
      } else {
        printer().error("No shortcuts found in the workspace.");
        printer().troubleshoot(
          "Use `sl <group>.<command> --save=<name>` to create a shortcut.");
        printer().troubleshoot("Use `sl --help` for more details.");
      }
    }
    printer().close_command();
  }

  // execute shortcuts
  for (const auto &shortcut : shortcut_list) {
    value = cli.get_option(shortcut.key());
    if (value) {
      const auto argument_list = value.split("?");
      const bool shortcut_result
        = execute_shortcut(Shortcut::load(shortcut.key()), argument_list);
      if (!shortcut_result) {
        return shortcut_result;
      }
    }
  }

  // check for the switches object
  const JsonValue shortcut_object
    = Group::workspace_settings().to_object().at("switches");
  if (shortcut_object.is_object()) {
    const auto key_list = shortcut_object.to_object().get_key_list();
    SL_PRINTER_TRACE("found switches object");
    for (const auto key : key_list) {
      value = cli.get_option(key);
      if (value) {
        const auto argument_list = value.split("?");
        const auto entry = shortcut_object.to_object().at(key);

        if (entry.is_string()) {
          if (execute_shortcut(entry.to_string(), argument_list) == false) {
            return false;
          }
        } else if (entry.is_array()) {
          for (size_t i = 0; i < entry.to_array().count(); i++) {
            const auto array_entry = entry.to_array().at(i);
            if (array_entry.is_string()) {
              if (
                execute_shortcut(array_entry.to_string(), argument_list)
                == false) {
                return false;
              }
            }
          }
        }
      }
    }
  }

  return result;
}

bool Switch::execute_shortcut(
  const StringView cmd,
  const StringViewList &argument_list) {
  bool result = true;
  String command(cmd);

  // check args required vs args provided
  const auto args_required = [](const StringView cmd) -> size_t {
    size_t i = 0;
    while (cmd.find(NumberString(i + 1, "?%d")) != StringView::npos) {
      i++;
    }
    return i;
  }(cmd);

  SL_PRINTER_TRACE("args required " | NumberString(args_required));
  SL_PRINTER_TRACE("args provided " | NumberString(argument_list.count()));

  if (args_required && (args_required != argument_list.count())) {
    printer().syntax_error(
      "shortcut requires " | NumberString(args_required) | " but "
      | NumberString(argument_list.count()) | " provided");
    return false;
  }

  if (args_required == 1 && argument_list.at(0) == "true") {
    printer().syntax_error("first argument is empty (or `true`)");
    return false;
  }

  SL_PRINTER_TRACE("Command is " + command);
  for (u32 i = 0; i < argument_list.count(); i++) {
    command(String::Replace()
              .set_old_string(String().format("?%d", i + 1))
              .set_new_string(argument_list.at(i)));
  }

  SL_PRINTER_TRACE("Command is now " + command);

  if (command != "<invalid>" && !command.is_empty()) {
    const Group::ExecutionStatus status
      = Group::execute_compound_command(command);
    if (status != Group::ExecutionStatus::success) {
      result = false;
    }
  }
  return result;
}

int Switch::show_changes() {

  auto show_list = [&](const StringView header, const StringViewList &list) {
    MarkdownPrinter::Header features_header(printer().markdown(), header);
    printer().newline();

    MarkdownPrinter::List markdown_list(
      printer().markdown(),
      MarkdownPrinter::ListType::unordered);

    int i = 1;
    for (const auto &item : list) {
      printer().markdown().key(NumberString(i++), item);
    }

    printer().newline();
  };

  auto get_version_header = [&](var::StringView version) {
    return MarkdownPrinter::Header(printer().markdown(), version);
  };

  {
    auto header = get_version_header("v1.20");
    printer().newline();

    show_list(
      "New Features",
      StringViewList()
        .push_back("Remove all references to `SOS_SDK_PATH` from the environment")
        .push_back("support `dest` when using `cloud.install` with a compiler sblob")
        .push_back("Cleaned up SDK. Pegged all dependencies to stable API "
                   "framework branchs")
        .push_back("Updated CMake to use `cmsdk2_` functions"));

    show_list(
      "Bug Fixes",
      StringViewList().push_back("Internal code cleanup and optimizations"));
  }

  {
    auto header = get_version_header("v1.18");
    printer().newline();

    show_list(
      "New Features",
      StringViewList()
        .push_back("Show the version in the bootloader attributes when using "
                   "`sl os.ping:boot`")
        .push_back("Add `encrypt/secure` option to bundle so that bundle files "
                   "can be encrypted")
        .push_back("Use API v1.3 with fixes for handling external Processes "
                   "and thread creation")
        .push_back("Add a workspace setting for the `preferredConnection`")
        .push_back("Add an option to `fs.bundle` to convert the binary output "
                   "to C source code"));

    show_list(
      "Bug Fixes",
      StringViewList()
        .push_back("fix `os.install` error with Stratify OS 4.2 bootloader")
        .push_back("fix `conn.connect:path=@serial` to connect to any "
                   "available serial port")
        .push_back("fix `app.ping:path=<directory>` bug that "
                   "didn't resolve the path correctly"));
  }

  {
    auto header = get_version_header("v1.17");
    printer().newline();

    show_list(
      "Bug Fixes",
      StringViewList().push_back("fix `cloud.install:compiler` on Windows"));
  }

  {
    auto header = get_version_header("v1.16");
    printer().newline();

    show_list(
      "New Features",
      StringViewList()
        .push_back(
          "When using `cloud.install`, `sync` is not `false` by default")
        .push_back("add `fs.bundle` which is exactly the same as `ux.bundle` "
                   "which will be removed in a later version")
        .push_back(
          "update `fs.archive` to encrypt/decrypt individual files using keys")
        .push_back("add `hash` option to `fs.download` to verify hashes match")
        .push_back("show sha256 hash of downloaded files")
        .push_back(
          "Add hash of original file when creating self extracting packages")
        .push_back("Add `encrypt` to `fs.archive`"));

    show_list(
      "Bug Fixes",
      StringViewList()
        .push_back("fix an `EINVAL` error when installing applications")
        .push_back("fix a major crashing bug with `app.clean`")
        .push_back("fix `fs.archive` and packager (broke in v1.15)"));
  }

  {
    auto header = get_version_header("v1.15");
    printer().newline();

    show_list(
      "New Features",
      StringViewList()
        .push_back("report an error if the packager `cloud.install:compler` "
                   "can't download the URL")
        .push_back(
          "remove dependency on `cmake` for external process execution")
        .push_back(
          "encode child system process output using base64 with json printer")
        .push_back("added `reconnect` as on option with `cloud.install`")
        .push_back(
          "Fixed issue when trying to connect to serial device based drivers")
        .push_back(
          "Fixed issue when download local application images from the cloud")
        .push_back("Use `~` for indeterminate progress using the JSON printer")
        .push_back("Add `signKey` as a workspace option to specify the default "
                   "signkey when installing applications or OS packages")
        .push_back("Add `app.install:orphan` option")
        .push_back("Use the specified filename as the embedded filename when "
                   "using `app.install:dest=host@...`"));

    show_list(
      "Bug Fixes",
      StringViewList()
        .push_back("remove `os.set` and `app.set`")
        .push_back("update the JSON output structure of various commands to "
                   "make them more consistent")
        .push_back("Do a better job of figuring out the architecture if it is "
                   "not specified")
        .push_back("Nothing yet"));
  }

  {
    auto header = get_version_header("v1.14");
    printer().newline();

    show_list(
      "New Features",
      StringViewList()
        .push_back("Add `keys.download` and allow key ids to be interpreted as "
                   "paths to `.json` files")
        .push_back("Show an error if the application needs to be signed but no "
                   "key is specified"));

    show_list(
      "Bug Fixes",
      StringViewList()
        .push_back("Fix bug when using `--update=preview`")
        .push_back("Fix bug when using `os.install` to install on a host path "
                   "(error with arch)"));
  }

  {
    auto header = get_version_header("v1.13");
    printer().newline();

    show_list("New Features", StringViewList().push_back("None from v1.12"));
  }

  {
    auto header = get_version_header("v1.12");
    printer().newline();

    show_list(
      "New Features",
      StringViewList()
        .push_back("Add switch for updating to the preview version using `sl "
                   "--update=preview`")
        .push_back("Add `auth` argument to `os.ping` to show the "
                   "authentication info of the target")
        .push_back("Add options to install commands to sign the images with a "
                   "key id and password")
        .push_back("Add options to install commands to insert public key in "
                   "bootloader image")
        .push_back("Add `--changes` switch")
        .push_back("Change default SDK directory to `SDK` (was `StratifySDK`) "
                   "when creating a workspace")
        .push_back(
          "Add command group `keys` or `key` for managing cryptographic keys"));

    show_list(
      "Bug Fixes",
      StringViewList().push_back(
        "Fix bug in 1.11 where reading elf files is truncating the output"));
  }

  {
    auto header = get_version_header("v1.11");
    printer().newline();

    show_list(
      "New Features",
      StringViewList()
        .push_back("Add `cloud.install:compiler,url=<url to os SDK>`")
        .push_back("Add `os.pack` to create a local SDK bundle that can be "
                   "uploaded to a "
                   "Github Release")
        .push_back("Update build system for self-contained build"));

    show_list(
      "Bug Fixes",
      StringViewList().push_back(
        "Initialize target byte buffers when opening terminal"));
  }

  return 0;
}

int Switch::show_readme(const var::StringView base_url) {
  printer().set_suppressed();
  {
    MarkdownPrinter::Header hg(
      printer().markdown(),
      String().format("Manual", VERSION));
    {
      MarkdownPrinter::Paragraph pg(printer().markdown());
      printer() << "`sl` version: " << VERSION;
      printer() << "\n\nUse `sl --readme > manual.md` to generate this manual "
                   "if your `sl` version is different.\n";
    }
    printer().newline();
    printer().horizontal_line().newline();
    {
      MarkdownPrinter::Paragraph pg(printer().markdown());
      printer() << "Welcome to the `sl` command line/cloud tool. `sl` is used "
                   "to develop, debug, deploy, and share "
                   "applications, OS packages and data for Stratify OS.";
    }
    printer().newline();
    {
      MarkdownPrinter::Header hg(printer().markdown(), "Workspaces and Login");
      {
        MarkdownPrinter::Paragraph pg(printer().markdown());
        printer()
          << "The `sl` command line tool operates in a workspace, or simply "
             "the current working directory. It automatically creates "
             "workspace files when they don't exist.";
      }
      {
        MarkdownPrinter::Paragraph pg(printer().markdown());
        printer() << "Because `sl` is a cloud tool, it requires you to login. "
                     "The first time you run `sl` in a new "
                     "workspace, it will launch the browser and ask you to "
                     "login. Once you create an account using the "
                     "[Stratify web application](https://app.stratifylabs.co), "
                     "you can copy an `sl` login command and "
                     "execute it in the terminal. This will create an "
                     "`sl_credentials.json` file in the workspace. The "
                     "command `sl cloud.logout` will remove the credentials "
                     "from the workspace.";
      }
    }
  }
  {
    MarkdownPrinter::Header hg(printer().markdown(), "Switches");
    {
      MarkdownPrinter::Paragraph pg(printer().markdown());
      printer() << "Switches can be passed with any command. All switches are "
                   "processed before any commands regardless "
                   "of the order.";
    }
    printer().newline();
    {
      MarkdownPrinter::List lg(
        printer().markdown(),
        MarkdownPrinter::ListType::unordered);
      const auto switch_list = Switch::get_list();
      for (const auto &sw : switch_list) {
        String switch_description;
        switch_description
          += "**" + sw.name() + "** \n" + sw.description() + "\n";
        printer() << switch_description.string_view();
        printer().newline();
      }
    }
  }
  {
    MarkdownPrinter::Header hg(printer().markdown(), "Commands");
    {
      MarkdownPrinter::Header hg(printer().markdown(), "Grammar");
      {
        MarkdownPrinter::Paragraph pg(printer().markdown());
        printer() << "Each `sl` command consists of a group and command "
                     "written `<group>.<command>[:<arguments>]`. Each "
                     "*group* has a long name and a short name. The long and "
                     "short names are expressed in this manual as "
                     "**long name|short name** (e.g, **connection|conn**). The "
                     "following commands are equivalent.";
      }
      {
        MarkdownPrinter::Code cg(printer().markdown(), "bash");
        printer() << "sl connection.ping";
        printer().newline();
        printer() << "sl conn.ping";
      }
      {
        MarkdownPrinter::Paragraph pg(printer().markdown());
        printer() << "Arguments are command specific and listed as "
                     "`<argument>=<value>`. Multiple arguments are "
                     "separated by commas. Use single quotes around a "
                     "`<value>` that includes commas or spaces.";
      }
      {
        MarkdownPrinter::Code cg(printer().markdown(), "bash");
        printer()
          << "sl "
             "application.install:path=HelloWorld,run=true,terminal=true";
        printer() << "sl filesystem.list:path=device@/home,recursive=true";
      }
      {
        MarkdownPrinter::Paragraph pg(printer().markdown());
        printer()
          << "For arguments with type `bool`, the value can be omitted and "
             "will be set to `true` if the "
             "argument is present. If a `bool` argument is omitted, it is "
             "assigned a default value which is "
             "specific to the argument and command (details for each "
             "argument "
             "are below). To specify a `bool` "
             "argument as false, it must be explicit: `recursive=false`.";
      }
      {
        MarkdownPrinter::Paragraph pg(printer().markdown());
        printer() << "Multiple commands sent on a single invocation of `sl` "
                     "are separated by spaces and executed from "
                     "left to right.";
      }
      {
        MarkdownPrinter::Code cg(printer().markdown(), "bash");
        printer()
          << "sl terminal.run:while=HelloWorld "
             "application.run:path=HelloWorld,args='--version' os.reset";
      }
    }
    {
      MarkdownPrinter::Header hg(printer().markdown(), "Output");
      {
        MarkdownPrinter::Paragraph pg(printer().markdown());
        printer() << "The output is in YAML format. The top level is an array "
                     "with each command passed being an element in the "
                     "array. Most commands output properly formatted YAML, but "
                     "some options may interfere. For example, if the "
                     "terminal is enabled to print to the standard output, the "
                     "YAML formatting will break down. Also, "
                     "`--verbose` levels of `message` and `debug` may also "
                     "break the formatting.";
      }
      {
        MarkdownPrinter::Code cg(printer().markdown(), "sh");
        printer() << "# connection.ping\n\n## output\n- "
                     "usb@/20A0/41D5/01/"
                     "0000000068004892E01ED1207F80F5B0:\n		- "
                     "name: StratifyToolbox\n		- serialNumber: "
                     "0000000068004892E01ED1207F80F5B0\n		- "
                     "hardwareId: 00000007\n		- projectId: \n	"
                     "	- bspVersion: 0.3\n		- sosVersion: "
                     "3.12.0d\n		- cpuArchitecture: v7em_f5dh\n	"
                     "	- cpuFrequency: 480000000\n		- "
                     "applicationSignature: 0\n		- bspGitHash: "
                     "ebb7b90\n		- sosGitHash: 870f968\n		- "
                     "mcuGitHash: b691e52\n- result: success\n";
        printer().close_code();
      }
    }
    {
      MarkdownPrinter::Header hg(printer().markdown(), "Exit Code");
      {
        MarkdownPrinter::Paragraph pg(printer().markdown());
        printer() << "If all commands complete successfully, the exit code is "
                     "`0`. If a command fails, the exit code is `1`. "
                     "This allows the use of the bash operators `&&` and `||` "
                     "to conditionally execute another command.";
      }
      {
        MarkdownPrinter::Code cg(printer().markdown(), "bash");
        printer() << "# reset if HelloWorld fails to install\n";
        printer() << "sl cloud.install:name=HelloWorld || sl os.reset\n";
        printer()
          << "# If the OS installs successfully, copy the README file\n";
        printer() << "sl os.install:path=Toolbox && sl"
                     "filesystem.copy:source=host@README.md,dest=device@/app/"
                     "flash/README.md";
      }
    }
    {
      MarkdownPrinter::Header hg(printer().markdown(), "Help");
      {
        MarkdownPrinter::Paragraph pg(printer().markdown());
        printer()
          << "All of the information in this manual is also available from "
             "the "
             "command line using the `--help` "
             "switch. For example:, `sl --help=application.install` will "
             "provide the relevant information for "
             "the [application.install](#applicationappinstall) command.";
      }
      printer().newline();
      {
        MarkdownPrinter::Code cg(printer().markdown(), "bash");
        printer() << "sl --help # general help information\n";
        printer() << "sl --help=application # list of commands available to "
                     "the application group\n";
        printer() << "sl --help=app # list of commands available to the "
                     "application group\n";
        printer() << "sl --help=application.install # details for the "
                     "application install command\n";
        printer() << "sl --help=app.install # same as above using short name\n";
      }
    }
    {
      MarkdownPrinter::Header hg(printer().markdown(), "Reference");
      {
        MarkdownPrinter::List lg(
          printer().markdown(),
          MarkdownPrinter::ListType::unordered);
        for (u32 i = 0; i < Group::group_list().count(); i++) {
          String name = String(Group::group_list().at(i)->m_name);
          String short_name
            = Group::group_list().at(i)->get_shortcut().to_string();

          auto command_list = Group::group_list().at(i)->get_command_list();

          if (name != short_name) {
            printer() << String()
                           .format(
                             "- [%s](%s#%s%s)\n",
                             name.cstring(),
                             PathString(base_url).cstring(),
                             name.cstring(),
                             short_name.cstring())
                           .string_view();
          } else {
            printer() << String()
                           .format(
                             "- [%s](%s#%s)\n",
                             name.cstring(),
                             PathString(base_url).cstring(),
                             name.cstring())
                           .string_view();
          }

          MarkdownPrinter::List lg(
            printer().markdown(),
            MarkdownPrinter::ListType::unordered);
          for (u32 idx = 0; idx < command_list.count(); idx++) {
            if (name != short_name) {
              printer() << String()
                             .format(
                               "  - [%s](#%s|%s.%s)\n",
                               PathString(command_list.at(idx)).cstring(),
                               name.cstring(),
                               short_name.cstring(),
                               PathString(command_list.at(idx)).cstring())
                             .string_view();
            } else {
              printer() << String()
                             .format(
                               "  - [%s.%s](#%s.%s)\n",
                               name.cstring(),
                               PathString(command_list.at(idx)).cstring(),
                               name.cstring(),
                               PathString(command_list.at(idx)).cstring())
                             .string_view();
            }
          }
        }
      }
    }
  }

  printer().newline();
  printer().horizontal_line().newline();

  for (u32 i = 0; i < Group::group_list().count(); i++) {
    String name = String(Group::group_list().at(i)->m_name);
    String short_name = Group::group_list().at(i)->get_shortcut().to_string();
    auto command_list = Group::group_list().at(i)->get_command_list();

    if (name != short_name) {
      printer().open_header(
        String().format("%s|%s", name.cstring(), short_name.cstring()));
    } else {
      printer().open_header(String().format("%s", name.cstring()));
    }
    // MarkdownParagraph pg(printer().markdown());

    for (const auto &command : command_list) {
      if (name != short_name) {
        printer().open_header(String().format(
          "%s|%s.%s",
          name.cstring(),
          short_name.cstring(),
          PathString(command).cstring()));
      } else {
        printer().open_header(String().format(
          "%s.%s",
          name.cstring(),
          PathString(command).cstring()));
      }
      printer() << "";
      Group::group_list().at(i)->execute_command(
        String().format("%s:help", PathString(command).cstring()));
      printer().close_header();
    }
    printer().close_header();
  }

  printer().close_object();
  printer().set_suppressed(false);

  return 0;
}
