#ifndef GROUPS_THING_HPP
#define GROUPS_THING_HPP

#include "Group.hpp"

class ThingGroup : public Group {
public:
  ThingGroup();

private:
  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool ping(const Command &command);

  enum commands { command_ping, command_total };
};

#endif // GROUPS_THING_HPP
