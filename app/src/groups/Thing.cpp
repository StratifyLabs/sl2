#include <var.hpp>

#include "Thing.hpp"

ThingGroup::ThingGroup() : Group("thing", "thing") {}

var::StringViewList ThingGroup::get_command_list() const {

  StringViewList list = {"ping"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool ThingGroup::execute_command_at(u32 list_offset, const Command &command) {

  switch (list_offset) {
  case command_ping:
    return ping(command);
  }
  return false;
}

bool ThingGroup::ping(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(insert, "pings the information for the specified thing.")
      + GROUP_ARG_OPT(
        identifier_id,
        string,
        <none>,
        "id (serial number) to ping."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView identifier = command.get_argument_value("identifier");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  if (identifier.is_empty()) {
    APP_RETURN_ASSIGN_ERROR("`identifier` or `serialnumber` must be specified");
  }

  Thing thing_document(identifier);

  if (!thing_document.is_existing()) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to download thing `" + identifier + "`");
  }

  printer().object(identifier, thing_document);

  return is_success();
}
