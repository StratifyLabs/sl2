// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef SCRIPT_HPP
#define SCRIPT_HPP

#include "Connector.hpp"

class Script : public Connector {
public:
  Script();

private:
  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool run(const Command &command);
  bool wait(const Command &command);
  bool execute_export(const Command &command);
  bool list(const Command &command);

  enum commands {
    command_run,
    command_wait,
    command_export,
    command_list,
    command_total
  };

  int find_case(json::JsonArray &test_array, const var::String &case_name);
  bool run_script_case(const var::String &group, const ScriptCase &sc);
  json::JsonObject load_script_object(const var::String &path);

  class ScriptNameCase {
    API_ACCESS_COMPOUND(ScriptNameCase, var::String, name);
    API_ACCESS_COMPOUND(ScriptNameCase, var::String, case_name);
  };

  ScriptNameCase
  split_name_case(const var::String &name, const var::String &case_name);
};

#endif // SCRIPT_HPP
