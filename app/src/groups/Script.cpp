// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <calc.hpp>
#include <var.hpp>

#include "DebugTrace.hpp"
#include "Script.hpp"
#include "Task.hpp"
#include "Terminal.hpp"

Script::Script() : Connector("script", "test") {}

var::StringList Script::get_command_list() const {
  StringList list = {"run", "wait", "export", "list"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool Script::execute_command_at(u32 list_offset, const Command &command) {
  switch (list_offset) {
  case command_run:
    return run(command);
  case command_wait:
    return wait(command);
  case command_export:
    return execute_export(command);
  case command_list:
    return list(command);
  }
  return false;
}

bool Script::list(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(list, "lists the scripts that are available.")
      + GROUP_ARG_OPT(
        name_n,
        string,
        <all>,
        "name of the script list (default is all).")
      + GROUP_ARG_OPT(
        path_p,
        string,
        <none>,
        "path to a JSON file that contains the script to list."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  String name = command.get_argument_value("name");
  String path = command.get_argument_value("path");

  command.print_options(printer());

  printer().open_output();

  JsonObject script_object;
  script_object = load_script_object(path);
  if (script_object.is_empty()) {
    return printer().close_fail();
  }

  Vector<ScriptGroup> script_list
    = script_object.construct_key_list<ScriptGroup>();

  Vector<String> name_list;
  if (name.is_empty() == false) {
    name_list = name.split("?");
  } else {
    name_list = script_object.keys();
  }

  for (const auto &name_in_list : name_list) {

    printer().open_object(name_in_list);

    u32 offset = script_list.find(ScriptGroup(name_in_list));
    if (offset == script_list.count()) {
      printer().close_object();
      printer().error(name_in_list + "does not exist in scripts");
      return printer().close_fail();
    }

    const ScriptGroup &group = script_list.at(offset);
    for (const ScriptCase &script_case : group.case_list()) {
      printer().key(script_case.get_case_name(), script_case.get_command());
    }
    printer().close_object();
  }

  return printer().close_success();
}

json::JsonObject Script::load_script_object(const var::String &path) {
  json::JsonObject result;
  if (path.is_empty() == false) {
    result = JsonDocument().load(fs::File::Path(path)).to_object();
    if (result.is_empty()) {
      printer().error("failed to load test from '%s'", path.cstring());
      return result;
    }
  } else {
    result = workspace_settings().get_script();
    if (result.is_empty()) {
      printer().error("no tests are defined in local workspace");
      return result;
    }
  }
  return result;
}

bool Script::run(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(run, "runs a test as defined in the local workspace.")
      + GROUP_ARG_OPT(case, string, <all>, "name of the case to run.")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "list the scripts to run without running.")
      + GROUP_ARG_OPT(
        name_n,
        string,
        <all>,
        "name of the script to run (default is to run all).")
      + GROUP_ARG_OPT(
        path_p,
        string,
        <none>,
        "path to a JSON file that contains the script."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  String name = command.get_argument_value("name");
  String path = command.get_argument_value("path");
  String dryrun = command.get_argument_value("dryrun");
  String script_case = command.get_argument_value("case");

  command.print_options(printer());

  printer().open_output();

  JsonObject script_object;
  script_object = load_script_object(path);
  if (script_object.is_empty()) {
    return printer().close_fail();
  }

  var::Vector<ScriptGroup> script_list;
  script_list = script_object.construct_key_list<ScriptGroup>();
  bool is_unresolved_references
    = (ScriptGroup::resolve_references(script_list) == false);

  if (script_list.count() == 0) {
    printer().error("no cases");
    return printer().close_fail();
  }

  Vector<String> name_list;
  if (name.is_empty() == false) {
    name_list = name.split("?");
  } else {
    name_list = script_object.keys();
  }

  for (const auto &name_in_list : name_list) {
    ScriptNameCase name_case = split_name_case(name_in_list, script_case);
    String script_name_item = name_case.name();
    String script_case_item = name_case.case_name();

    u32 script_group_offset = script_list.find(ScriptGroup(script_name_item));
    if (script_group_offset == script_list.count()) {
      printer().error("test '%s' does not exist", script_name_item.cstring());
      return printer().close_fail();
    }

    printer().open_array(script_name_item, printer::Printer::Level::info);

    SL_PRINTER_TRACE("run all test cases");

    const ScriptGroup &group = script_list.at(script_group_offset);

    for (const ScriptCase &sc : group.case_list()) {
      String current_case = sc.get_case_name();
      SL_PRINTER_TRACE(
        "check case `" + script_case_item + "` == `" + current_case + "`");

      // process all or process this specific case
      if (script_case_item.is_empty() || script_case_item == current_case) {

        if (sc.reference().is_specified()) {
          if (sc.reference().is_valid()) {
            printer().output_key(
              sc.reference().reference_case()->get_case_name(),
              String().format(
                "refer to -> %s.%s",
                sc.reference().get_name().cstring(),
                sc.reference().get_case_name().cstring()));
          } else {
            printer().output_key(
              sc.reference().get_name() + "." + sc.reference().get_case_name(),
              String("reference not found"));
          }
        } else {
          printer().output_key("preparing", current_case);
        }
      }
    }
    printer().close_array();
  }

  if (is_unresolved_references) {
    printer().warning("unresolved references");
  }

  printer().close_success();

  if (dryrun == "true") {
    return true;
  }

  bool is_something_failed = false;

  for (const auto &name_in_list : name_list) {
    ScriptNameCase name_case = split_name_case(name_in_list, script_case);
    String script_name_item = name_case.name();
    String script_case_item = name_case.case_name();

    u32 offset = script_list.find(ScriptGroup(script_name_item));
    if (offset == script_list.count()) {
      for (const ScriptGroup &group : script_list) {
        SL_PRINTER_TRACE("list has " + JsonDocument().stringify(group.value()));
      }
      printer().open_command("script.finalize");
      printer().open_output();
      printer().open_object(script_name_item);
      printer().error(script_name_item + " was not found");
      return printer().close_fail();
    }

    SL_PRINTER_TRACE("run all test cases");
    for (ScriptCase &sc : script_list.at(offset).case_list()) {
      String current_case = sc.get_case_name();
      SL_PRINTER_TRACE(String().format(
        "check case '%s' == '%s'",
        script_case.cstring(),
        sc.get_case_name().cstring()));

      // case is either not specified or is a match
      if (
        script_case_item.is_empty() || script_case_item == sc.get_case_name()) {
        SL_PRINTER_TRACE(
          String().format("run test case '%s'", current_case.cstring()));

        ScriptCase *scp = nullptr;
        if (sc.reference().is_specified()) {
          scp = sc.reference().reference_case();
          SL_PRINTER_TRACE(String().format("reference points to %p", scp));
        } else {
          scp = &sc;
          SL_PRINTER_TRACE(String().format("object points to %p", scp));
        }

        if (scp == nullptr) {
          continue;
        }

        if (run_script_case(script_name_item, *scp) == false) {
          // was true expected
          if (scp->is_result()) {
            if (scp->is_fatal()) {
              // exit now
              printer().open_command("script.final");
              printer().open_output();
              printer().error(String().format(
                "test `%s.%s` fatal error",
                script_name_item.cstring(),
                current_case.cstring()));
              return printer().close_fail();
            }

            if (scp->is_assert()) {
              // aborting cases in this group
              printer().open_output();
              printer().warning(String().format(
                "assert failed on `%s.%s`, skipping cases in group",
                script_name_item.cstring(),
                current_case.cstring()));
              printer().insert_newline();
              break;
            } else {
              is_something_failed = true;
            }
          }
        }
      }
    }
  }

  if (is_something_failed) {
    printer().open_command("script.final");
    printer().open_output();
    printer().error("not all tests passed");
    return printer().close_fail();
  }

  printer().open_command("script.final");
  printer().open_output();
  return printer().close_success();
}

bool Script::run_script_case(const var::String &group, const ScriptCase &sc) {

  bool is_terminal = sc.is_terminal();
  if (sc.is_debug()) {
    SL_PRINTER_TRACE("script has debug on");
    if (sc.is_terminal() == false) {
      printer().warning("terminal required with debug enabling now");
      is_terminal = true;
    }
  }

  PrinterHeader run_header(printer(), group + "." + sc.get_case_name());
  {
    PrinterOptions po(printer());
    printer().option_key("command", sc.get_command());
    printer().option_key(
      "clean",
      sc.get_clean().is_empty() ? "<none>" : sc.get_clean().cstring());
    printer().option_key(
      "arguments",
      sc.get_arguments().is_empty() ? "<none>" : sc.get_arguments().cstring());
    printer().option_key("result", sc.is_result());
    printer().option_key(
      "duration",
      String::number(sc.duration().seconds(), F32U " seconds"));
    printer().option_key("analyze", sc.is_analyze());
    printer().option_key("terminal", is_terminal);
    printer().option_key("showTerminal", sc.is_show_terminal());
    printer().option_key("json", sc.is_json());
    printer().option_key("debug", sc.is_debug());
    printer().option_key("count", String::number(sc.get_count()));
  }

  SL_PRINTER_TRACE("checking arguments");
  String case_command = sc.get_command();
  if (!sc.get_arguments().is_empty() && sc.get_arguments() != "<invalid>") {
    case_command << ",args=" << sc.get_arguments();
  }

  SL_PRINTER_TRACE("updated command is " + case_command);

  for (int i = 0; i < sc.get_count(); i++) {
    PrinterHeader count_header(
      printer(),
      String().format("progress %d of %d", i + 1, sc.get_count()));

    SL_PRINTER_TRACE("execute " + case_command);
    int status = GroupCommand(case_command).execute();
    if (status != Group::execution_status_success) {
      printer().close_object(); // close case
      printer().close_object(); // close count
      return false;
    }

    if (is_terminal) {
      String terminal_output;
      String terminal_command_string;
      String debug_command_string;
      // monitor the terminal output and put it in the test report

      Terminal terminal;
      Task task;
      DebugTrace debug(terminal);

      task.set_update_period(chrono::Milliseconds(100));
      task.set_duration(chrono::MicroTime(0));
      terminal.set_update_period(chrono::Milliseconds(100));

      terminal.initialize();
      task.initialize();

      task.set_update_period(chrono::Milliseconds(1000));
      terminal.minimum_duration() = chrono::Milliseconds(5000);

      // write the output to a string rather than a file or the stdout
      terminal.set_output(terminal_output);

      char termination = ':';
      terminal_command_string << "terminal.run";
      debug_command_string << "debug.trace";
      if (sc.get_while_name().is_empty() == false) {
        terminal_command_string << ":while=" << sc.get_while_name();
        termination = ',';
      }

      terminal_command_string
        << termination
        << "duration=" << String::number(sc.duration().seconds());
      debug_command_string << ":duration="
                           << String::number(sc.duration().seconds());

      SL_PRINTER_TRACE("run terminal with " + terminal_command_string);
      if (
        terminal.execute_run(
          Command(Command::Group("terminal"), terminal_command_string))
        == false) {
        // failed to run terminal with arguments
        printer().error(
          "failed to run terminal with '%s'",
          terminal_command_string.cstring());
        return false;
      }

      if (sc.is_debug()) {
        String command_string = String("debug.trace:duration=");
        debug.execute_trace(Command(Command::Group("debug"), command_string));
        debug.initialize();
      }

      if (sc.is_analyze()) {
        String command_string = String("task.analyze");
        task.analyze(Command(Command::Group("task"), command_string));
      }

      SL_PRINTER_TRACE(String().format(
        "wait for terminal to complete %d",
        terminal.is_running()));

      while (terminal.update()) {
        if (sc.is_debug()) {
          debug.update();
        }

        task.update();
        chrono::wait(chrono::Milliseconds(10));
      }

      if (terminal.finalize() == false) {
        SL_PRINTER_TRACE("failed to finalize terminal");
      }

      // this outputs gibberish -- very bizarre bug

      if (sc.is_show_terminal()) {
        printer().open_terminal_output();
        printf("\n%s", terminal_output.cstring());
        printer().close_terminal_output();
      }

      if (terminal_output.length()) {
        String base64_terminal_output = Base64::encode(terminal_output);

        if (printer().is_json()) {
          printer().key("terminal.content", base64_terminal_output);
        } else {
          printer().open_code();
          printer().insert_newline();
          printer() << terminal_output;
          printer().close_code();
        }
        printer().insert_newline();
      }

      task.finalize();

      if (sc.is_debug()) {
        SL_PRINTER_TRACE(
          String().format("finalize debug %d", debug.is_running()));
        debug.finalize();
        SL_PRINTER_TRACE(
          String().format("debug finalized %d", debug.is_running()));
      }
    }

    if (sc.get_clean().is_empty() == false) {
      status = GroupCommand(sc.get_clean()).execute();
      if (status != Group::execution_status_success) {
        printer().error("failed to run clean command `" + sc.get_clean() + "`");
        printer().insert_newline();
        if (sc.is_fatal()) {
          return false;
        }
      }
    }
  }

  return true;
}

bool Script::wait(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(wait, "delays the specified amount.")
      + GROUP_ARG_REQ(
        milliseconds_ms,
        string,
        <milliseconds>,
        "amount of time to wait in milliseconds."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  String milliseconds = command.get_argument_value("milliseconds");

  command.print_options(printer());

  printer().open_output();
  printer().key("wait", milliseconds);
  chrono::wait(chrono::Milliseconds(milliseconds.to_integer()));
  return printer().close_success();
}

bool Script::execute_export(const Command &command) {
  printer().open_command("script.export");

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      export,
      "exports the tests in the local workspace to a file.")
      + GROUP_ARG_REQ(name_n, string, <name>, "name of the output file."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  String name = command.get_argument_value("name");

  if (name.find((".json")) == String::npos) {
    name << ".json";
  }

  command.print_options(printer());

  printer().open_output();
  json::JsonObject test_object = workspace_settings().script();
  printer() << test_object;

  if (JsonDocument().save(test_object, fs::File::Path(name)) < 0) {
    printer().error("failed to save test to '%s'", name.cstring());
    return printer().close_fail();
  }

  return printer().close_success();
}

Script::ScriptNameCase
Script::split_name_case(const var::String &name, const var::String &case_name) {
  String script_name_item = name;
  String script_case_item = case_name;

  if (script_case_item.is_empty()) {
    StringList split_name = name.split(".");
    if (split_name.count() > 1) {
      script_name_item = split_name.at(0);
      script_case_item = split_name.at(1);
    }
  }
  return ScriptNameCase()
    .set_name(script_name_item)
    .set_case_name(script_case_item);
}

int Script::find_case(
  json::JsonArray &test_array,
  const var::String &case_name) {
  for (u32 i = 0; i < test_array.count(); i++) {
    if (test_array.at(i).to_object().at(("case")).to_string() == case_name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}
