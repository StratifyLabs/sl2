#ifndef DEVICES_FLASH_HPP
#define DEVICES_FLASH_HPP

#include "Connector.hpp"

class FlashGroup : public Connector {
public:
  FlashGroup();

private:
  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool ping(const Command &command);
  bool write(const Command &command);
  bool erase(const Command &command);

  enum commands { command_ping, command_write, command_erase, command_total };
};

#endif // DEVICES_FLASH_HPP
