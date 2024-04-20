#ifndef GROUP_MCU_HPP
#define GROUP_MCU_HPP

#include "Group.hpp"

class Mcu : public Group {
public:
  Mcu();

private:
  enum commands { command_process, command_parse, command_bundle, command_total };

  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool process(const Command &command);
  bool parse(const Command &command);
  bool bundle(const Command &command);

  bool process_stm32(const Command &command);
  bool process_lpc(const Command &command);


  class ParseStm32Interrupts {
    API_AC(ParseStm32Interrupts, PathString, source);
    API_AC(ParseStm32Interrupts, PathString, destination);
  };

  bool parse_stm32_interrupts(const ParseStm32Interrupts &options);
};

#endif // GROUP_MCU_HPP
