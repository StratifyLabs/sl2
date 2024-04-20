// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef TERMINAL_HPP
#define TERMINAL_HPP

#include <chrono.hpp>
#include <fs.hpp>
#include <hal.hpp>
#include <inet.hpp>
#include <sos.hpp>
#include <thread.hpp>

#include <cstdio>

#include "../utilities/Updater.hpp"

class Terminal : public Updater {
public:
  Terminal();
  ~Terminal();

  bool initialize();
  bool finalize();
  bool update() override;
  Terminal &stop();

  Terminal &set_output(var::String &output) {
    m_output = &output;
    return *this;
  }

  const chrono::MicroTime &period() const { return update_period(); }
  bool execute_run(const Command &command);
  void process_input();
  bool is_redirected() const { return m_output != nullptr; }

  chrono::MicroTime &minimum_duration() { return m_minimum_duration; }
  const chrono::MicroTime &minimum_duration() const {
    return m_minimum_duration;
  }

private:
  enum commands { command_run, command_listen, command_connect, command_total };

  ByteBuffer m_device_input;  // keyboard -> device
  ByteBuffer m_device_output; // device -> display

  int m_while_pid = 0;
  int m_while_pid_counter = 0;
  var::NameString m_while_name;
  int m_loop_count = 100000;
  var::String *m_output = nullptr;
  fs::DataFile m_output_file;
  u32 m_cummulative_allowed = 1024 * 250;
  bool m_is_display = false;
  bool m_is_save_output = false;
  fs::File m_log_file;
  chrono::MicroTime m_minimum_loop_delay = 100_milliseconds;
  chrono::MicroTime m_minimum_duration = 1_seconds;

  StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;
  bool execute_listen(const Command &command);
  bool execute_connect(const Command &command);
};

#endif // TERMINAL_HPP
