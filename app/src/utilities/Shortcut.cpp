// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <var.hpp>

#include "Group.hpp"
#include "Shortcut.hpp"
#include "Switch.hpp"

bool Shortcut::save(const var::StringView name, const var::StringView command) {

  // names can't be other switches
  const auto switch_list = Switch::get_list();
  for (u32 i = 0; i < switch_list.count(); i++) {
    if (switch_list.at(i).name() == name) {
      return false;
    }
  }

  ShortcutInfoList shortcut_list
    = Group::workspace_settings().get_shortcut_list();

  shortcut_list.push_back(ShortcutInfo(name, JsonString(command)));

  Group::workspace_settings().set_shortcut_list(shortcut_list);

  return true;
}

var::String Shortcut::load(const var::StringView name) {
  return Group::workspace_settings().shortcut_list().at(name).to_string();
}
