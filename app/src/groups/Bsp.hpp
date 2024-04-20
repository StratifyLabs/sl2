// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef BSP_HPP
#define BSP_HPP

#include "Connector.hpp"

class Bsp : public Connector {
public:
  Bsp();

private:
  enum commands {
    command_install,
    command_invoke_bootloader,
    command_reset,
    command_publish,
    command_configure,
    command_ping,
    command_authenticate,
    command_pack,
    command_total
  };

  API_NO_DISCARD var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool install(const Command &command);
  bool invoke_bootloader(const Command &command);
  bool reset(const Command &command);
  bool publish(const Command &command);
  bool configure(const Command &command);
  bool ping(const Command &command);
  bool pack(const Command &command);
  bool authenticate(const Command &command);
};

#endif // BSP_HPP
