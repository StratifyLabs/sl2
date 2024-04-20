#include <var.hpp>

#include "HardwareGroup.hpp"

HardwareGroup::HardwareGroup() : Group("hardware", "hw") {}

var::StringViewList HardwareGroup::get_command_list() const {

  StringViewList list = {"ping", "publish"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool HardwareGroup::execute_command_at(
  u32 list_offset,
  const Command &command) {

  switch (list_offset) {
  case command_ping:
    return ping(command);
  case command_publish:
    return publish(command);
  }
  return false;
}

bool HardwareGroup::ping(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(ping, "pings the information about the hardware.")
      + GROUP_ARG_OPT(hardware_id, string, <current>, "id to ping."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView identifier = command.get_argument_value("hardware");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  if (identifier.is_empty()) {
    identifier = cloud_service().cloud().credentials().get_uid();
  }

  SlPrinter::Output printer_output_guard(printer());
  Hardware hw_document(identifier);

  if (is_error()) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to download hardware `" + identifier + "`");
  }

  if( hw_document.is_existing() == false ){
    APP_RETURN_ASSIGN_ERROR("hardware `" + identifier + "` does not exist");
  }

  printer().object(identifier, hw_document.to_object());

  return is_success();
}

bool HardwareGroup::publish(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(publish, "publish a hardware description document.")
      + GROUP_ARG_REQ(
        path_p,
        string,
        <path to json file>,
        "path to the JSON file that describes the hardware."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView path = command.get_argument_value("path");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(path);
  Hardware hw_document = Hardware().import_file(File(path));

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("failed to load hardware description from " + path);
  }

  hw_document.save();

  if (is_error()) {
    printer().error(
      "failed to publish hardware `" + hw_document.get_document_id() + "`");
    return printer().close_fail();
  }

  printer().object("published", hw_document.to_object());

  return is_success();
}
