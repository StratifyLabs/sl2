// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved

#include <sys.hpp>
#include <var.hpp>

#include "Task.hpp"
#include "Terminal.hpp"

Terminal::Terminal() : Updater("terminal", "term") {}

Terminal::~Terminal() {}

bool Terminal::initialize() {

  return true;
}

bool Terminal::finalize() {


  if (is_initialized()) {
    printer().open_command(GROUP_COMMAND_NAME);
    GROUP_ADD_SESSION_REPORT_TAG();
    SlPrinter::Output printer_output_guard(printer());
    if (m_device_output.fileno() >= 0) {
      SL_PRINTER_TRACE("remove write block on terminal");
      m_device_output.set_attributes(
        ByteBuffer::Attributes().set_writeblock().set_overflow());

      if (is_error()) {
        printer().warning("failed to set stdout in non-blocking mode");
        API_RESET_ERROR();
      }
    }

    // force deconstruction
    m_device_output = ByteBuffer();
    m_device_input = ByteBuffer();

    printer().close_success();

  }

  return true;
}

bool Terminal::update() {
  if (is_time_to_update()) {
    // read the device output
    process_input();

    if ((m_while_pid < 0) && !m_while_name.is_empty()) {
      m_while_pid = Task::get_pid_from_name(m_while_name);
    }

    if (
      (m_while_pid < 0)
      && (duration_timer().micro_time() > m_minimum_duration)) {
      // the task was never started after the minimum duration
      stop_timers();
    } else {

      if (Task::is_pid_running(m_while_pid) == false) {
        if (m_minimum_duration < duration_timer()) {
          m_while_pid_counter++;
          if (m_while_pid_counter > 50) {
            stop_timers();
          }
        } else {
          m_while_pid = -1;
        }
      } else {
        m_while_pid_counter = 0;
      }
    }
  }

  return is_running();
}

void Terminal::process_input() {
  Array<char, 8192> output_buffer;
  u32 cummulative = 0;
  int bytes_read = 0;
  do {
    if (connection()->is_connected_and_is_not_bootloader()) {
      bytes_read
        = m_device_output.read(View(output_buffer).pop_back(1)).return_value();
    }

    if (is_error()) {
      API_RESET_ERROR();
    } else if (bytes_read > 0) {

      // printf("got %d bytes\n", bytes_read);
      output_buffer.at(bytes_read) = 0;
      StringView output(View(output_buffer).to_const_char(), bytes_read);

      m_output_file.write(output);
      push_queue(output.to_string());

      if (m_output != nullptr) {
        *m_output += output;
      } else {

        if (m_is_display) {
          printf("%s", output_buffer.data());
          fflush(stdout);
        }

        if (m_log_file.fileno() >= 0) {
          m_log_file.write(output);
        }
      }
      cummulative += bytes_read;
    }

  } while ((bytes_read > 0) && (cummulative < m_cummulative_allowed));
}

Terminal &Terminal::stop() {
  stop_timers();
  return *this;
}

bool Terminal::execute_command_at(u32 list_offset, const Command &command) {
  if (list_offset == command_run) {
    return execute_run(command);
  }
  return false;
}

bool Terminal::execute_run(const Command &command) {

  printer().open_command("terminal.run");

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      run,
      "reads/writes the stdio of the connected device and display it on the "
      "host computer terminal. The terminal will "
      "run until ^C is pushed.")
      + GROUP_ARG_OPT(append_a, bool, false, "append data to the log file.")
      + GROUP_ARG_OPT(
        display_d,
        bool,
        true,
        "display the terminal data on the host terminal output (set to 'false' "
        "and use 'log' to just log to a file).")
      + GROUP_ARG_OPT(
        duration,
        int,
        <indefinite>,
        "duration in seconds to run the terminal.")
      + GROUP_ARG_OPT(
        log_l,
        string,
        <filename>,
        "path to a file where the terminal data will be written.")
      + GROUP_ARG_OPT(
        timestamp_ts,
        bool,
        <false>,
        "prefix the log file with a unique timestamp.")
      + GROUP_ARG_OPT(
        period,
        int,
        10,
        "polling duration in milliseconds. Polling the terminal requires CPU "
        "processing on the connected device. Use a "
        "larger value here to limit, the device processing time spent on "
        "servicing this command.")
      + GROUP_ARG_OPT(
        while_w,
        string,
        ,
        "application name to watch. The terminal will stop when the "
        "application terminates.")

  );

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  const auto display = command.get_argument_value("display");
  const auto log_file_path = command.get_argument_value("log");
  const auto while_is_running = command.get_argument_value("while");
  const auto is_append_log = command.get_argument_value("append");
  const auto period = command.get_argument_value("period");
  const auto duration = command.get_argument_value("duration");
  const auto timestamp = command.get_argument_value("timestamp");

  // open the stdio device
  command.print_options(printer());

  if (is_connection_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  s32 milliseconds;

  set_duration(duration.to_integer() * 1_seconds);

  milliseconds = period.to_integer();
  if (milliseconds < 2) {
    milliseconds = 2;
  }
  set_update_period(milliseconds * 1_milliseconds);

  if (while_is_running.is_empty() == false) {

    m_while_name = while_is_running;

    // set the pid for the running app
    m_while_pid = -1;
  }

  m_is_display = (display == "true");

  if (log_file_path.is_empty() == false) {

    if (is_append_log == "true") {

      m_log_file
        = File(log_file_path, fs::OpenMode::append_write_only()).move();

      if (is_error()) {
        APP_RETURN_ASSIGN_ERROR(
          "failed to open log file " + log_file_path + " for appending");
      }

    } else {
      const auto parent_directory = Path::parent_directory(log_file_path);
      const auto log_file_name = Path::name(log_file_path);
      const auto ts_log_file_name
        = (timestamp == "true") ? (
            ClockTime::get_system_time().to_unique_string().string_view() & "-"
            & log_file_name)
                                : PathString(log_file_name);
      const auto ts_log_file_path = parent_directory.is_empty()
                                      ? ts_log_file_name
                                      : parent_directory / ts_log_file_name;

      m_log_file = File(fs::File::IsOverwrite::yes, ts_log_file_path).move();

      if (is_error()) {
        APP_RETURN_ASSIGN_ERROR("failed to create log file " + log_file_path);
      }
    }
  }

  if (while_is_running.is_empty() && duration.is_empty()) {
    printer().info("press ctrl+c to stop terminal");
  }

  m_device_input = ByteBuffer(
                     connection()->info().stdin_name(),
                     fs::OpenMode::write_only().set_non_blocking(),
                     connection()->driver());

  m_device_output = ByteBuffer(
                      connection()->info().stdout_name(),
                      fs::OpenMode::read_only().set_non_blocking(),
                      connection()->driver());

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR(
      String("failed to open stdio device ") + error().message());
  }

  // set the output fifo's to write block
  m_device_output.set_attributes(ByteBuffer::Attributes().set_initialize())
    .set_attributes(ByteBuffer::Attributes().set_writeblock());

  m_device_input.set_attributes(ByteBuffer::Attributes().set_initialize());
  start_timers();

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("failed to configure stdio");
  }

  return is_success();
}

bool Terminal::execute_listen(const Command &command) {
  Command reference(
    Command::Group(get_name()),
    "listen=port:req_string|Port number to listen for incoming "
    "connections||term.listen:port=4444|");

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  if (is_connection_ready() == false) {
    return printer().close_fail();
  }

  return printer().close_fail();
}

bool Terminal::execute_connect(const Command &command) {
  Command reference(
    Command::Group(get_name()),
    "connect=port:req_string|Port number to listen for incoming "
    "connections||term.connect:port=4444|,host:opt_string_localhost|Host to "
    "connect "
    "to|term.connect:host=localhost,port=4444|");

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  if (is_connection_ready() == false) {
    return printer().close_fail();
  }

  return printer().close_fail();
}

var::StringViewList Terminal::get_command_list() const {
  StringViewList list = {"run", "listen", "connect"};
  API_ASSERT(list.count() == command_total);

  return list;
}
