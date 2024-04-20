// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include "Settings.hpp"
#include "Command.hpp"
#include "Group.hpp"
#include <sys.hpp>
#include <var.hpp>

Settings::Settings() : Group("settings", "sett") {}

bool Settings::execute_command_at(u32 list_offset, const Command &command) {


  switch (list_offset) {
  case command_list:
    return list(command);
  case command_set:
    return set(command);
  }

  return false;
}

var::StringViewList Settings::get_command_list() const {

  StringViewList list = {"list", "set"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool Settings::list(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(list, "lists workspace settings.")
      + GROUP_ARG_OPT(
        global_g,
        bool,
        false,
        "list values from global settings.")
      + GROUP_ARG_OPT(
        key_k,
        string,
        <all>,
        "list only the values under the *key*."));

  if (!command.is_valid(reference, printer())) {

    return printer().close_fail();
  }

  StringView key = command.get_argument_value("key");
  StringView is_global = command.get_argument_value("global");

  if (is_global.is_empty()) {
    is_global = "false";
  }

  command.print_options(printer());

    SlPrinter::Output printer_output_guard(printer());
  if (key.is_empty()) {
    // list all the settings
    if (is_global == "true") {
      printer().object("global", global_settings());
    } else {
      printer().object("workspace", workspace_settings().to_object());
    }
  } else {
    if (is_global == "true") {
      printer().key(key, global_settings().to_object().at((key)));
    } else {
      printer().key(key, workspace_settings().to_object().at((key)));
    }
  }

  return is_success();
}

var::String Settings::external_set_reference(const var::StringView group) {

  String reference_string;
  reference_string += "set:description=opt_string|The `" + group
                      + ".set` command sets the key to the specified value.||"
                      + group + ".set:key=<key>,value=<value>|";
  reference_string += ",key_k=req_string|key for the *value* to be set.||"
                      + group + ".set:key=<key>,value=<value>|";
  reference_string += ",value_v=req_string|value to assign to *key*.||" + group
                      + ".set:key=<key>,value=<value>|";
  if (group != "settings") {
    reference_string
      += ",path_p=req_string|path to the application or OS package.||" + group
         + ".set:path=HelloWorld,key=<key>,value=<value>|";
  } else {
    reference_string
      += ",global_g=opt_bool_false|set the key in global settings.||" + group
         + ".set:global,key=<key>,value=<value>|";
  }
  reference_string
    += ",add=opt_bool_false|the key will be added if it doesn't exist.||"
       + group + ".set:key=<key>,value=<value>,add=true|";

  return reference_string;
#if 0
	Command reference(
				Command::Group(group),
				reference_string);
#endif
}

bool Settings::execute_external_set(
  const Command &command,
  json::JsonObject &object,
  bool is_allow_add,
  SlPrinter &printer,
  const var::StringView group) {

  Command reference(Command::Group(group), external_set_reference(group));

  if (!command.is_valid(reference, printer)) {
    return false;
  }

  StringView global = command.get_argument_value("global");
  StringView key = command.get_argument_value("key");
  StringView value = command.get_argument_value("value");
  StringView is_add = command.get_argument_value("add");

  command.print_options(printer);
  printer.open_output();

  bool is_save = false;
  if (object.is_empty()) {
    is_save = true;
    if (global == "true") {
      object = global_settings();
      printer.open_object("global");
    } else {
      object = workspace_settings();
      printer.open_object("workspace");
    }
  }

  if (is_add.is_empty()) {
    is_add = "false";
  }

  if (!is_allow_add && is_add == "true") {
    printer.error("add not allowed");
    return false;
  }

  // does the key exist
  auto keys = object.get_key_list();

  if (is_add == "false") {
    if (keys.find_offset(key) == keys.count()) {
      printer.error(key + " is not a valid key");
      return false;
    }
  }

  printer.key(key, value);
  object.insert(key, json::JsonString(value));
  if (object.return_value() < 0) {
    return false;
  }

  return true;
}

bool Settings::execute_external_list(
  const Command &command,
  json::JsonObject &object,
  SlPrinter &printer,
  const var::String &group) {

  String reference_string;

  reference_string
    += "list:key_k=opt_string|List values filtered by key||list:key=uid|";
  if (group != "settings") {
    reference_string
      += ",path_p=req_string|Path to the application or BSP project||" + group
         + ".set:path=HelloWorld,key=<key>,value=<value>|";
  }

  Command reference(Command::Group(command.group()), reference_string);

  if (!command.is_valid(reference, printer)) {
    if (command.get_argument_value("help") == "true") {
      return true;
    }
    return false;
  }

  const StringView key = command.get_argument_value("key");

  if (key.is_empty()) {
    // list all the settings
    auto key_list = object.get_key_list();
    for (const auto key : key_list) {
      printer.key(key, object.at(key).to_string());
    }

  } else {
    printer.key(key, object.at((key)).to_string());
  }

  return true;
}

bool Settings::set(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  JsonObject object;
  if (
    execute_external_set(command, object, true, printer(), "settings")
    == false) {
    return printer().close_fail();
  }

  return is_success();
}
