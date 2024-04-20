// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <json/Json.hpp>

#include "../Group.hpp"

class Command;

class Settings : public Group {
public:
  Settings();

  int initialize();
  static var::String external_set_reference(const StringView group);
  static bool execute_external_set(
    const Command &command,
    json::JsonObject &object,
    bool is_allow_add,
    SlPrinter &printer,
    const StringView group);
  static bool execute_external_list(
    const Command &command,
    json::JsonObject &object,
    SlPrinter &printer,
    const var::String &group);

private:
  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool list(const Command &command);
  bool set(const Command &command);
  bool remove(const Command &command);

  enum commands { command_list, command_set, command_total };
};

#endif // SETTINGS_HPP
