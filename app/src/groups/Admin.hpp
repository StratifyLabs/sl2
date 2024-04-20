// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef GROUP_ADMIN_HPP
#define GROUP_ADMIN_HPP

#include "Group.hpp"

class Admin : public Group {
public:
	Admin();

private:
  enum commands { command_upload, command_import, command_total };

  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;
  bool upload(const Command &command);
  bool import(const Command &command);
  bool gather(const Command &command);

};

#endif // GROUP_ADMIN_HPP
