#include <chrono.hpp>
#include <var.hpp>

#include "DeviceExecutionContext.hpp"

DeviceExecutionContext::DeviceExecutionContext() : m_debug(m_terminal) {
  set_update_period(100_milliseconds);
}

bool DeviceExecutionContext::execute(const Options &options) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("DeviceExecutionContext");
  if (start(options) && wait(options)) {
    return true;
  }
  return false;
}

bool DeviceExecutionContext::start(const Options &options) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("DeviceExecutionContext");
  SL_PRINTER_TRACE("checking arguments");
  String case_command = String("app.run:path=") + options.application();
  if (!options.arguments().is_empty()) {
    case_command += ",args=" + options.arguments();
  }

  SL_PRINTER_TRACE("command is " + case_command);

  SL_PRINTER_TRACE("execute " + case_command);
  Group::ExecutionStatus status = Group::CommandInput(case_command).execute();

  if (status != Group::ExecutionStatus::success) {
    printer().close_object(); // close case
    printer().close_object(); // close count
    return false;
  }

  return true;
}

bool DeviceExecutionContext::wait(const Options &options) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("DeviceExecutionContext");

  String terminal_command_string;
  String debug_command_string;
  // monitor the terminal output and put it in the test report

  m_terminal_output.clear();

  m_task.set_update_period(update_period());
  m_task.set_duration(chrono::MicroTime(0));
  m_terminal.set_update_period(update_period());

  m_terminal.initialize();
  m_task.initialize();

  m_task.set_update_period(1_seconds);
  m_terminal.minimum_duration() = 5_seconds;

  // write the output to a string rather than a file or the stdout
  m_terminal.set_output(m_terminal_output);

  StringView termination = ":";
  terminal_command_string += "terminal.run";
  debug_command_string += "debug.trace";
  terminal_command_string += ":while=" + options.application();
  termination = ",";

  terminal_command_string
    += termination + "duration=" + NumberString(duration().seconds());
  debug_command_string += ":duration=" + NumberString(duration().seconds());

  SL_PRINTER_TRACE("run terminal with " + terminal_command_string);
  if (
    m_terminal.execute_run(
      Command(Command::Group("terminal"), terminal_command_string))
    == false) {
    // failed to run terminal with arguments
    printer().error("failed to run terminal with " + terminal_command_string);
    return false;
  }

  if (is_debug()) {
    String command_string = String("debug.trace:duration=");
    m_debug.execute_trace(Command(Command::Group("debug"), command_string));
    m_debug.initialize();
  }

  if (is_analyze()) {
    String command_string = String("task.analyze");
    m_task.analyze(Command(Command::Group("task"), command_string));
  }

  SL_PRINTER_TRACE(String().format(
    "wait for terminal to complete %d",
    m_terminal.is_running()));

  while (m_terminal.update()) {
    if (is_debug()) {
      m_debug.update();
    }

    m_task.update();
    chrono::wait(10_milliseconds);
  }

  if (m_terminal.finalize() == false) {
    SL_PRINTER_TRACE("failed to finalize terminal");
  }

  if (is_show_terminal()) {
    printer().open_terminal_output();
    printf("\n%s", m_terminal_output.cstring());
    printer().close_terminal_output();
  }

  if (m_terminal_output.length()) {
    String base64_terminal_output = Base64().encode(m_terminal_output);

    if (printer().is_json()) {
      printer().key("terminal.content", base64_terminal_output);
    } else {
      printer().open_code();
      printer().insert_newline();
      printer() << m_terminal_output.string_view();
      printer().close_code();
    }
    printer().insert_newline();
  }

  m_task.finalize();

  if (is_debug()) {
    SL_PRINTER_TRACE(
      String().format("finalize debug %d", m_debug.is_running()));
    m_debug.finalize();
    SL_PRINTER_TRACE(
      String().format("debug finalized %d", m_debug.is_running()));
  }

  return true;
}
