#ifndef TEAMGROUP_HPP
#define TEAMGROUP_HPP

#include "Group.hpp"

class TeamGroup : public Group {
public:
  TeamGroup();

private:
  enum commands {
    command_ping,
    command_add,
    command_update,
    command_create,
    command_total
  };

  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool ping(const Command &command);
  bool add(const Command &command);
  bool update(const Command &command);
  bool create(const Command &command);
};

#endif // TEAMGROUP_HPP
