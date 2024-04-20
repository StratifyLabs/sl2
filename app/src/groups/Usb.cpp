// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <sys.hpp>
#include <var.hpp>

#include "Task.hpp"
#include "Usb.hpp"

UsbGroup::UsbGroup() : Connector("usb", "usb") {}

var::StringViewList UsbGroup::get_command_list() const {

  StringViewList list = {"list", "ping"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool UsbGroup::execute_command_at(u32 list_offset, const Command &command) {

  switch (list_offset) {
  case command_list:
    return list(command);
  case command_ping:
    return ping(command);
  }

  return false;
}

bool UsbGroup::list(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      list,
      "executes a benchmark test and reports the results to the cloud for the "
      "connected device.")
      + GROUP_ARG_OPT(
        identifier_id,
        string,
        <id>,
        "cloud ID of the bench test to install and run.")
      + GROUP_ARG_OPT(
        team,
        string,
        <public>,
        "the team ID of the project to install (default is to install a public "
        "project)."));

  // connect by name
  // connect by serial number
  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  StringView id = command.get_argument_value("identifier");
  StringView team = command.get_argument_value("team");

  command.print_options(printer());

  return is_success();
}

bool UsbGroup::ping(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      list,
      "executes a benchmark test and reports the results to the cloud for the "
      "connected device.")
      + GROUP_ARG_REQ(
        path,
        string,
        <vid> / <pid> / <interface> / <serial number>,
        "specify the path. Leave blank for wildcard (for example "
        "`/4124/1034//123455648325`)")
      + GROUP_ARG_OPT(
        winusb,
        bool,
        false,
        "ping the device to see if it has valid WIN USB OS descriptors."));

  // connect by name
  // connect by serial number
  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  StringView id = command.get_argument_value("identifier");
  StringView team = command.get_argument_value("team");

  command.print_options(printer());

  return is_success();
}
