#ifndef USERGROUP_HPP
#define USERGROUP_HPP

#include "Group.hpp"

class UserGroup : public Group {
public:
  UserGroup();

private:
  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool ping(const Command &command);

  enum commands { command_ping, command_total };
};

#endif // USERGROUP_HPP
