#ifndef GROUP_KEYS_GROUP_HPP
#define GROUP_KEYS_GROUP_HPP

#include "Group.hpp"

class KeysGroup : public Group {
public:
  KeysGroup();

private:
  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool ping(const Command &command);
  bool publish(const Command &command);
  bool sign(const Command &command);
  bool verify(const Command &command);
  bool revoke(const Command &command);
  bool remove(const Command &command);
  bool download(const Command&command);

  enum commands {
    command_ping,
    command_publish,
    command_sign,
    command_verify,
    command_revoke,
    command_remove,
    command_download,
    command_total
  };
};

#endif // GROUP_KEYS_GROUP_HPP
