// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef DEBUGTRACE_HPP
#define DEBUGTRACE_HPP

#include <swd/Elf.hpp>

#include "Terminal.hpp"
#include "utilities/Updater.hpp"

class DebugTrace : public Updater {
public:
  explicit DebugTrace(const Terminal &terminal);

  bool initialize();
  bool finalize();
  bool update() override;
  bool execute_trace(const Command &command);

private:
  enum commands { command_trace, command_analyze, command_total };

  hal::FrameBuffer m_trace;
  const Terminal &m_terminal;

  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;
  bool analyze(const Command &command);

  PathString get_address_function(
    const swd::Elf::SymbolList &os_symbol_list,
    const swd::Elf::SymbolList &application_symbol_list,
    u32 address) const;
};

#endif // DEBUGTRACE_HPP
