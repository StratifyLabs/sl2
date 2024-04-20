// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef GROUPS_USB_HPP
#define GROUPS_USB_HPP

#include "Cloud.hpp"
#include "Connector.hpp"
#include "Terminal.hpp"

class UsbGroup : public Connector {
public:
  UsbGroup();

private:
  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool list(const Command &command);
  bool ping(const Command &command);

  enum commands { command_list, command_ping, command_total };
};

#endif // GROUPS_USB_HPP
