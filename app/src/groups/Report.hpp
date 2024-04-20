// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef REPORT_HPP
#define REPORT_HPP

#include "Connector.hpp"

class ReportGroup : public Connector {
public:
  ReportGroup();

private:
  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool publish(const Command &command);
  bool list(const Command &command);
  bool remove(const Command &command);
  bool ping(const Command &command);
  bool parse(const Command &command);
  bool download(const Command &command);

  enum commands {
    command_publish,
    command_list,
    command_remove,
    command_ping,
    command_parse,
    command_download,
    command_total
  };
};

#endif // SCRIPT_HPP
