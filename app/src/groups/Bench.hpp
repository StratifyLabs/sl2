// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef BENCH_HPP
#define BENCH_HPP

#include "Cloud.hpp"
#include "Connector.hpp"
#include "Terminal.hpp"

class Bench : public Connector {
public:
  Bench();

private:
  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool test(const Command &command);

  enum commands { command_test, command_total };
};

#endif // BENCH_HPP
