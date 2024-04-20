// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef GROUP_HPP
#define GROUP_HPP

#include "App.hpp"
#include "Command.hpp"

#define GROUP_COMMAND_NAME (get_name() + "." + __FUNCTION__)

#define GROUP_ADD_SESSION_REPORT_TAG() (add_session_report_tag(__FUNCTION__))

#define GROUP_ARG_DESC(a, desc)                                                \
  var::String(MCU_STRINGIFY(a) ":description=opt_string|" desc "|| |")

#define GROUP_ARG_OPT(a, t, def, des)                                          \
  var::String("," MCU_STRINGIFY(a) "=opt_" MCU_STRINGIFY(t) "_" MCU_STRINGIFY( \
    def) "|" des "|| |")

#define GROUP_ARG_REQ(a, t, def, des)                                          \
  var::String("," MCU_STRINGIFY(a) "=req_" MCU_STRINGIFY(t) "_" MCU_STRINGIFY( \
    def) "|" des "|| |")

class GroupFlags {
public:
  enum class ExecutionStatus { skipped, failed, success, syntax_error };
};

class Group;

class Group : public GroupFlags, public AppAccess {
public:
  class CommandInput : public GroupFlags, public AppAccess {
    API_AC(CommandInput, var::String, name);
    API_AC(CommandInput, var::String, command);
    API_AF(CommandInput, Group *, group, nullptr);

  public:
    CommandInput(const StringView group_command);
    bool is_valid() const { return group() != nullptr; }
    ExecutionStatus execute();
  };

  Group(const char *name, const char *shortcut);
  virtual ~Group();

  static ExecutionStatus
  execute_compound_command(const var::String &compound_command);
  static bool execute_cli_arguments(const sys::Cli &cli);
  static bool
  execute_command_list(var::Vector<CommandInput> &command_input_list);
  static void add_group(Group &group);

  void handle_document_cloud_error();

  var::StringView get_name() const { return m_name; }
  var::StringView get_shortcut() const {
    if (m_shortcut != nullptr) {
      return m_shortcut;
    }
    return var::StringView();
  }

  static const var::Vector<Group *> &group_list() { return m_group_list; }
  static Group *get_group(const var::StringView name) {
    for (auto group : group_list()) {
      if (group->m_name == name) {
        return group;
      }
      if (group->m_shortcut == name) {
        return group;
      }
    }
    return nullptr;
  }

protected:
  bool
  is_directory_valid(const StringView path, bool is_suppress_error = false);

  void add_session_report_tag(const var::StringView command) {
    session_settings().add_tag(Document::Tag().set_key("sl").set_value(
      (get_name() + "." + command).string_view()));

    if (
      session_settings().group_list().find_offset(String(get_name()))
      == session_settings().group_list().count()) {
      session_settings().group_list().push_back(String(get_name()));
    }
  }

  // String & error_message(){ return m_error_message; }
private:
  friend class GroupCommand;
  friend class Switch;
  static var::Vector<Group *> m_group_list;

  const char *m_name;
  const char *m_shortcut;

  static bool handle_switches(const sys::Cli &cli);

  ExecutionStatus execute_command(const var::String &command_string);
  virtual var::StringViewList get_command_list() const = 0;
  virtual bool execute_command_at(u32 list_offset, const Command &command) = 0;
};

#endif // GROUP_HPP
