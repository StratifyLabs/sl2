#ifndef GROUP_HARDWARE_GROUP_HPP
#define GROUP_HARDWARE_GROUP_HPP

#include "Group.hpp"

class HardwareGroup : public Group {
public:
  HardwareGroup();

private:
  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool ping(const Command &command);
  bool publish(const Command &command);

  enum commands { command_ping, command_publish, command_total };
};

#endif // GROUP_HARDWARE_GROUP_HPP
