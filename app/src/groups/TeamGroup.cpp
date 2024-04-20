#include <var.hpp>

#include "TeamGroup.hpp"

TeamGroup::TeamGroup() : Group("team", "team") {}

var::StringViewList TeamGroup::get_command_list() const {
  StringViewList list = {"ping", "add", "update", "create"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool TeamGroup::execute_command_at(u32 list_offset, const Command &command) {
  switch (list_offset) {
  case command_ping:
    return ping(command);
  case command_add:
    return add(command);
  case command_update:
    return update(command);
  case command_create:
    return create(command);
  }
  return false;
}

bool TeamGroup::add(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(add, "add a user to a team.")
      + GROUP_ARG_REQ(user, string, <user id>, "user to add")
      + GROUP_ARG_OPT(create, bool, false, "set user create permissions")
      + GROUP_ARG_OPT(remove, bool, false, "set user remove permissions")
      + GROUP_ARG_OPT(read, bool, true, "set user read permissions")
      + GROUP_ARG_OPT(write, bool, false, "set user write permissions")
      + GROUP_ARG_OPT(admin, bool, false, "set user admin permissions")
      + GROUP_ARG_REQ(
        team_id,
        string,
        <team id>,
        "team id for user to update."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView identifier = command.get_argument_value("team");
  StringView user_id = command.get_argument_value("user");
  StringView create = command.get_argument_value("create");
  StringView remove = command.get_argument_value("remove");
  StringView read = command.get_argument_value("read");
  StringView write = command.get_argument_value("write");
  StringView admin = command.get_argument_value("admin");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object("add");

  SL_PRINTER_TRACE("adding user to team");
  Team::User user = Team::User(identifier, user_id)
                      .set_create(create == "true")
                      .set_permissions("private")
                      .set_document_id(user_id)
                      .set_user_id(user_id)
                      .set_team_id(identifier)
                      .set_remove(remove == "true")
                      .set_read(read == "true")
                      .set_write(write == "true")
                      .set_admin(admin == "true")
                      .save();

  SL_PRINTER_TRACE("user was added");

  if (is_error()) {
    SL_PRINTER_TRACE("failed to create user " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to create user");
  }

  printer().key("user", user.id()).key_bool("preExisting", user.is_existing());

  return is_success();
}

bool TeamGroup::update(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(update, "update a user on the team")
      + GROUP_ARG_REQ(user, string, <user id>, "user to update")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "show changes to be made without making them")
      + GROUP_ARG_OPT(create, bool, <no change>, "set user create permissions")
      + GROUP_ARG_OPT(remove, bool, <no change>, "set user remove permissions")
      + GROUP_ARG_OPT(read, bool, <no change>, "set user read permissions")
      + GROUP_ARG_OPT(write, bool, <no change>, "set user write permissions")
      + GROUP_ARG_OPT(admin, bool, <no change>, "set user admin permissions")
      + GROUP_ARG_REQ(
        team_id,
        string,
        <team id>,
        "team id for user to update."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView team_id = command.get_argument_value("team");
  StringView user_id = command.get_argument_value("user");
  StringView create = command.get_argument_value("create");
  StringView remove = command.get_argument_value("remove");
  StringView write = command.get_argument_value("write");
  StringView read = command.get_argument_value("read");
  StringView admin = command.get_argument_value("admin");
  StringView dryrun = command.get_argument_value("dryrun");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  Team::User user(team_id, user_id);

  if (is_error()) {
    return printer().close_fail();
  }

  printer().object("current", user);

  if (!create.is_empty()) {
    user.set_create(create == "true");
  }

  if (!remove.is_empty()) {
    user.set_remove(remove == "true");
  }

  if (!write.is_empty()) {
    user.set_write(write == "true");
  }

  if (!admin.is_empty()) {
    user.set_admin(admin == "true");
  }

  if (!read.is_empty()) {
    user.set_read(read == "true");
  }

  printer().object("updated", user);

  if (dryrun != "true") {

    // CloudAPI requires all docs have a permissions entry
    user.set_permissions("private").save();

    if (is_error()) {
      return printer().close_fail();
    }
  }

  return is_success();
}

bool TeamGroup::ping(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(ping, "pings information for the specified team.")
      + GROUP_ARG_OPT(
        user,
        string,
        <none>,
        "list details about a user on the team")
      + GROUP_ARG_REQ(team_id, string, <team id>, "id to ping."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView identifier = command.get_argument_value("team");
  StringView user_id = command.get_argument_value("user");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  Team team_document(identifier);

  if (team_document.is_existing() == false) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to download team `" + identifier + "`");
  }

  printer().object(identifier, team_document.to_object());

  if (user_id.is_empty() == false) {
    Team::User user(identifier, user_id);

    if (user.is_existing() == false) {
      APP_RETURN_ASSIGN_ERROR("failed to download user `" + user_id + "`");
    }

    printer().object("user", user);
  }

  return is_success();
}

bool TeamGroup::create(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      create,
      "creates a new team (you must be a pro user to create team).")
      + GROUP_ARG_REQ(name, string, <none>, "The name of the team")
      + GROUP_ARG_OPT(
        permissions,
        string,
        private,
        "Permissions for viewing the team")
      + GROUP_ARG_OPT(user, string, <current>, "ID of the owning user"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView name = command.get_argument_value("name");
  StringView user = command.get_argument_value("user");
  StringView permissions = command.get_argument_value("permissions");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object("team");

  Team team = Team()
                .set_name(name)
                .set_permissions(permissions)
                .set_user_id(
                  (user.is_empty() ? cloud_service().cloud().credentials().get_uid() : user));

  team.save();

  if (is_error()) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to create team");
  }

  printer().key("id", team.id());

  return is_success();
}
