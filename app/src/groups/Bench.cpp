// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <sys.hpp>
#include <var.hpp>

#include "Bench.hpp"
#include "Task.hpp"

Bench::Bench() : Connector("benchmark", "bench") {}

var::StringViewList Bench::get_command_list() const {

  StringViewList list = {"test"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool Bench::execute_command_at(u32 list_offset, const Command &command) {

  switch (list_offset) {
  case command_test:
    return test(command);
  }

  return false;
}

bool Bench::test(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      test,
      "executes a benchmark test and reports the results to the cloud for the "
      "connected device.")
      + GROUP_ARG_REQ(
        identifier_id,
        string,
        <id>,
        "cloud ID of the bench test to install and run.")
      + GROUP_ARG_OPT(
        team,
        string,
        <public>,
        "the team ID of the project to install (default is to install a public "
        "project).")
      + GROUP_ARG_OPT(
        external_ext,
        bool,
        <false>,
        "install the code and data in external RAM (ignored if `ram` is "
        "`false`).")
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
        ram,
        bool,
        <auto>,
        "Install the code in RAM rather than flash (can't be used with "
        "'startup').")
      + GROUP_ARG_OPT(
        tightlycoupled_tc,
        bool,
        <false>,
        "Install the code and data in tightly coupled RAM (ignored if `ram` is "
        "`false`).")
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

  // connect by name
  // connect by serial number
  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  StringView id = command.get_argument_value("identifier");
  StringView team = command.get_argument_value("team");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  if (is_connection_ready() == false) {
    return printer().close_fail();
  }

  // first download the project and make sure it is a bench test
  Project project(id);
  if (is_error()) {
    SlPrinter::Output printer_output_guard(printer());
    return printer().close_fail();
  }

  StringView tags = project.to_object().at("tags").to_string().string_view();
  if (tags.find("bench") == String::npos) {
    printer().warning("this is not a bench test");
  }

  // install the project
  {
    SlPrinter::Output printer_output_guard(printer());
    printer().open_object("install | " + id);
    Installer(connection())
      .install(
        get_installer_options(command).set_build_name("release").set_clean());

    if (is_error()) {
      APP_RETURN_ASSIGN_ERROR("failed to install " + id);
    }
  }

  // execute the test
  Terminal terminal;
  Task task;
  String terminal_output;

  task.initialize();
  terminal.initialize();
  terminal.set_output(terminal_output);
  terminal.execute_run(Command(
    Command::Group("terminal"),
    "terminal.run:period=100,while=" + project.get_name()));

  // flush the terminal output
  // terminal.update();
  // terminal_output.clear();

  // start executing the application
  ExecutionStatus status;
  status = Group::CommandInput(
             "application.run:path=" + project.get_name()
             + ",args='--performance --all'")
             .execute();
  if (status != Group::ExecutionStatus::success) {
    return printer().close_fail();
  }

  while (terminal.update()) {
    task.update();
    chrono::wait(500_milliseconds);
    SL_PRINTER_TRACE("waiting for benchmark to complete");
  }

  if (terminal.finalize() == false) {
    SL_PRINTER_TRACE("failed to finalize terminal");
  }

  if (task.finalize() == false) {
    SL_PRINTER_TRACE("failed to finalize terminal");
  }

  printer().open_command("bench.finalize");
  SlPrinter::Output printer_output_guard(printer());

  // parse the JSON output of the terminal to determine the results
  JsonObject output = JsonDocument().from_string(terminal_output);

  JsonValue::KeyList keys = output.get_key_list();

  SL_PRINTER_TRACE(String().format("output has %d keys", keys.count()));
  printer().key(
    "serialNumber",
    connection()->info().serial_number().to_string());

  if (output.at(("system")).to_object().is_empty()) {
    APP_RETURN_ASSIGN_ERROR("bench is missing system information");
  }

  if (output.at(("test")).to_object().is_empty()) {
    APP_RETURN_ASSIGN_ERROR("bench is missing test information");
  }

  // create a report

  return is_success();
}
