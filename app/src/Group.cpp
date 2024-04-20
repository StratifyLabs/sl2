// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved

#include <chrono.hpp>
#include <fs.hpp>
#include <inet.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "groups/Cloud.hpp"
#include "groups/Settings.hpp"
#include "settings/Credentials.hpp"

#include "utilities/LocalServer.hpp"
#include "utilities/Shortcut.hpp"
#include "utilities/Switch.hpp"

#include "Group.hpp"

#define ARCHIVE_WORKSPACE_HISTORY 0

Vector<Group *> Group::m_group_list;

Group::CommandInput::CommandInput(const var::StringView group_command) {
  size_t pos = group_command.find(".");
  if (pos == var::String::npos) {
    set_name(group_command.to_string());
    return;
  }
  set_name(group_command.get_substring_with_length(pos).to_string());
  set_command(group_command.get_substring_at_position(pos + 1).to_string());

  for (Group *group_pointer : Group::m_group_list) {
    if (
      (group_pointer->get_name() == name())
      || (group_pointer->get_shortcut() == name())) {
      set_group(group_pointer);
    }
  }
}

GroupFlags::ExecutionStatus Group::CommandInput::execute() {
  if (is_valid()) {
    return group()->execute_command(command());
  }
  return ExecutionStatus::syntax_error;
}

Group::Group(const char *name, const char *shortcut) {
  m_name = name;
  m_shortcut = shortcut;
}

Group::~Group() {}

void Group::add_group(Group &group) { m_group_list.push_back(&group); }

GroupFlags::ExecutionStatus
Group::execute_command(const String &command_string) {
  ExecutionStatus status = ExecutionStatus::syntax_error;
  // does the cli command match this group name
  APP_CALL_GRAPH_TRACE_FUNCTION();

  SL_PRINTER_TRACE("construct command from " + command_string);
  Command command(Command::Group(get_name()), command_string);
  // does the command name match a command from the group
  const StringViewList command_list = get_command_list();
  bool is_found = false;
  size_t i = 0;
  for (const auto command_name : command_list) {
    if (command_name == command.name()) {
      is_found = true;
      if (execute_command_at(i, command)) {
        status = ExecutionStatus::success;
      } else {
        status = ExecutionStatus::failed;
      }
    }
    i++;
  }

  if (!is_found) {
    SL_PRINTER_TRACE("Didn't find command name " + command.name());
  }

  return status;
}

GroupFlags::ExecutionStatus
Group::execute_compound_command(const var::String &compound_command) {
  ExecutionStatus status = ExecutionStatus::success;
  StringViewList list
    = Tokenizer(
        compound_command,
        Tokenizer::Construct().set_delimiters(" ").set_ignore_between("\"'"))
        .list();

  for (const auto &item : list) {
    if (!item.is_empty()) {
      status = CommandInput(item).execute();
      if (status != ExecutionStatus::success) {
        SL_PRINTER_TRACE(item | "failed to execute");
        return status;
      }
    }
  }
  return status;
}

bool Group::execute_cli_arguments(const Cli &cli) {
  SL_PRINTER_TRACE_PERFORMANCE();
  APP_CALL_GRAPH_TRACE_FUNCTION();

  if (Switch::handle_switches(cli) == false) {
    return false;
  }

  SL_PRINTER_TRACE("parse CLI command");
  var::Vector<CommandInput> group_command_list;
  group_command_list.reserve(cli.count());
  for (u32 i = 1; i < cli.count(); i++) {
    if (cli.at(i).length() && cli.at(i).at(0) != '-') {
      group_command_list.push_back(CommandInput(cli.at(i)));
    }
  }

  return execute_command_list(group_command_list);
}

bool Group::execute_command_list(
  var::Vector<CommandInput> &command_input_list) {
  for (const auto &command_input : command_input_list) {
    if (command_input.is_valid() == false) {
      printer().syntax_error("`" |
        command_input.name() | "` is not a recognized group");
      return false;
    }
  }

  for (CommandInput &group_command : command_input_list) {
    if (group_command.is_valid() == false) {

      printer().troubleshoot(
        "You can use `sl --help` to show a list of command groups");

      printer().syntax_error(
        "`" + group_command.name() + "` is not a recocognized command group");
    }
  }

  if (
    workspace_settings().is_valid() == false
    && (command_input_list.count() > 0)) {
    printer().open_command("workspace");
    SlPrinter::Output printer_output_guard(printer());
    if (workspace_settings().is_file_present()) {
      printer().object("parse", workspace_settings().load_error());
      printer().troubleshoot(
        "Please fix the parsing error detailed above in the `"
        + FilePathSettings::workspace_settings_filename() + "` file.");
      APP_RETURN_ASSIGN_ERROR(
        "`" + FilePathSettings::workspace_settings_filename()
        + "` is not properly formed JSON");
    } else {
      printer().troubleshoot(
        "Use `sl --initialize` to initialize the current directory "
        "as "
        "an `sl` workspace.");
      APP_RETURN_ASSIGN_ERROR("Current directory is not a valid workspace");
    }
  }

  // check if this is a cloud job
  bool result = true;
  ExecutionStatus status = ExecutionStatus::skipped;
  if (workspace_settings().job_info().get_job_id().is_empty() == false) {
    var::String command;
    for (const CommandInput &input : command_input_list) {
      if( (input.name() != "cloud")
          || ((input.command().string_view().find("listen") == String::npos)
							&& ((input.command().string_view().find("connect") == String::npos)))){
        command += input.name() + "." + input.command() + " ";
      }
      if (command.length()) {
        command.pop_back();
        status = CloudGroup::publish_job(command);
      }
    }
  }

  if (status == ExecutionStatus::skipped) {

    for (CommandInput &input : command_input_list) {
      ExecutionStatus status;
      status = input.execute();
      if (status != ExecutionStatus::success) {
        SL_PRINTER_TRACE("status is execution failed");
        if (status == ExecutionStatus::syntax_error) {
          printer().syntax_error(input.command() + " has a syntax error");
        }
        result = false;
        break;
      }
    }
  }

  return result;
}

void Group::handle_document_cloud_error() {
  // to do
}

bool Group::is_directory_valid(
  const var::StringView path,
  bool is_suppress_error) {
  if (FileSystem().directory_exists(path) == false) {
    if (is_suppress_error == false) {
      printer().error("`" + path + "` is not a valid directory");
    }
    return false;
  }
  return true;
}
