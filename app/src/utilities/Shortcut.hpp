// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef SHORTCUT_HPP
#define SHORTCUT_HPP

#include <json/Json.hpp>
#include <var/String.hpp>
#include <var/Vector.hpp>

#include "../settings/WorkspaceSettings.hpp"

class Shortcut {
public:
  static bool save(const var::StringView name, const var::StringView command);

  static var::String load(const var::StringView name);
  static json::JsonKeyValueList<ShortcutInfo> list() {
    return Group::workspace_settings().get_shortcut_list();
  }



};

#endif // SHORTCUT_HPP
