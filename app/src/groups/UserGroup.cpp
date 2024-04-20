#include <var.hpp>

#include "UserGroup.hpp"

UserGroup::UserGroup() : Group("user", "user") {}

var::StringViewList UserGroup::get_command_list() const {

  StringViewList list = {"ping"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool UserGroup::execute_command_at(u32 list_offset, const Command &command) {

  switch (list_offset) {
  case command_ping:
    return ping(command);
  }
  return false;
}

bool UserGroup::ping(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(ping, "pings the information about the user.")
      + GROUP_ARG_OPT(identifier_id, string, <current>, "id to ping."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView identifier = command.get_argument_value("identifier");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  if (identifier.is_empty()) {
    identifier = cloud_service().cloud().credentials().get_uid();
  }

  SlPrinter::Output printer_output_guard(printer());
  User user_document(identifier);

  if (user_document.is_existing() == false) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to download user `" + identifier + "`");
  }

  printer().object(identifier, user_document.to_object());
  return is_success();
}
