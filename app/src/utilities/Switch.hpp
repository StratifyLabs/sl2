#ifndef SWITCH_HPP
#define SWITCH_HPP

#include "App.hpp"
#include <sys/Cli.hpp>

class Switch : public AppAccess {
public:
  Switch(const var::StringView name, const var::StringView description)
    : m_name(name), m_description(description) {}

  static bool handle_switches(const sys::Cli &cli);

  static var::Vector<Switch> get_list();

private:
  API_ACCESS_COMPOUND(Switch, var::String, name);
  API_ACCESS_COMPOUND(Switch, var::String, description);

  static int show_changes();
  static int show_readme(const StringView base_url);
  static bool handle_shortcuts(const sys::Cli &cli);

  static bool execute_shortcut(const StringView command, const StringViewList& argument_list);

};

#endif // SWITCH_HPP
