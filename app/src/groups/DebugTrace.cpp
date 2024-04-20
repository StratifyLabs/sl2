// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include "DebugTrace.hpp"
#include <fs.hpp>
#include <printer.hpp>
#include <sos/Trace.hpp>
#include <swd/Elf.hpp>
#include <var.hpp>

#if !defined __win32
#include <cxxabi.h>
#endif

DebugTrace::DebugTrace(const Terminal &terminal)
  : m_terminal(terminal), Updater("debug", "dbug") {}

bool DebugTrace::initialize() {

  if (is_running()) {
    printer().open_command(GROUP_COMMAND_NAME);
    GROUP_ADD_SESSION_REPORT_TAG();

#if defined NOT_BUILDING
    m_trace.set_driver(connection()->driver());
    SlPrinter::Output printer_output_guard(printer());

    String device_name = connection()->sys_info().trace_name();
    // see if device has a debug trace that is available
    if (device_name.is_empty()) {
      printer().error("target does not have a trace output");
      return printer().close_fail();
    }

    if (m_trace.open(device_name) < 0) {
      printer().error("failed to open trace output %s", device_name.cstring());
      return printer().close_fail();
    }
    SL_PRINTER_TRACE(
      String().format("opened trace fileno %d", m_trace.fileno()));
    return is_success();
#endif
  }

  return is_running();
}

bool DebugTrace::finalize() {

  if (is_initialized()) {
    printer().open_command(GROUP_COMMAND_NAME);
    GROUP_ADD_SESSION_REPORT_TAG();
    SlPrinter::Output printer_output_guard(printer());

#if defined NOT_BUILDING
    SL_PRINTER_TRACE("closing debug trace connection");
    if (m_trace.fileno() >= 0) {
      SL_PRINTER_TRACE(
        String().format("close debug trace fileno %d", m_trace.fileno()));
      m_trace.close();
    } else {
      SL_PRINTER_TRACE(String().format(
        "failed to close debug trace (%d)",
        m_trace.error_number()));
    }
#endif

    set_initialized(false);
    stop_timers();
    return is_success();
  }

  return true;
}

bool DebugTrace::update() {
  if (is_time_to_update()) {

    sos::TraceEvent trace_event;
    hal::FrameBuffer::Info info = m_trace.get_info();

    if (info.frame_count_ready() > 0) {

      if (m_terminal.is_running() && !m_terminal.is_redirected()) {
        printer().close_terminal_output();
      }

      for (u32 i = 0; i < info.frame_count_ready(); i++) {
        m_trace.read(var::View(trace_event.event()));

        // change the color
#if 0
        printer().set_color_code(printer::Printer::COLOR_CODE_LIGHT_GREEN);
        switch (trace_event.id()) {
        case LINK_POSIX_TRACE_FATAL:
        case LINK_POSIX_TRACE_CRITICAL:
          printer().set_color_code(printer::Printer::COLOR_CODE_RED);
          break;
        case LINK_POSIX_TRACE_ERROR:
          printer().set_color_code(printer::Printer::COLOR_CODE_MAGENTA);
          break;
        case LINK_POSIX_TRACE_WARNING:
          printer().set_color_code(printer::Printer::COLOR_CODE_YELLOW);
          break;
        case LINK_POSIX_TRACE_MESSAGE:
          printer().set_color_code(printer::Printer::COLOR_CODE_LIGHT_GREEN);
          break;
        default:
          printer().set_color_code(printer::Printer::COLOR_CODE_CYAN);
          break;
        }
#else

#endif
        printer().open_command("debug.trace");
        SlPrinter::Output printer_output_guard(printer());
        printer() << trace_event;
        if (
          (trace_event.id() == LINK_POSIX_TRACE_FATAL)
          || (trace_event.id() == LINK_POSIX_TRACE_CRITICAL)
          || (trace_event.id() == LINK_POSIX_TRACE_ERROR)) {
          SL_PRINTER_TRACE("close fail");
          printer().close_fail();
        } else {
          SL_PRINTER_TRACE("close success");
          printer().close_success();
        }

        // change the color back
      }
    }

    // printer().clear_color_code();

    if (m_terminal.is_running() && !m_terminal.is_redirected()) {
      printer().open_terminal_output();
    }
  }

  return is_running();
}

var::StringViewList DebugTrace::get_command_list() const {
  StringViewList list = {"trace", "analyze"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool DebugTrace::execute_command_at(u32 list_offset, const Command &command) {
  switch (list_offset) {
  case command_trace:
    return execute_trace(command);
  case command_analyze:
    return analyze(command);
  }
  return false;
}

bool DebugTrace::execute_trace(const Command &command) {

  printer().open_command("debug.trace");

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      trace,
      "causes `sl` to monitor the trace output of the system and display any "
      "messages in the terminal.")
      + GROUP_ARG_OPT(
        duration,
        int,
        <indefinite>,
        "duration of time in seconds to run the debug trace.")
      + GROUP_ARG_OPT(
        enabled,
        bool,
        true,
        "monitor the system debug trace output.")
      + GROUP_ARG_OPT(
        period,
        int,
        100,
        "sample period in milliseconds of the debug tracing buffer."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView enabled = command.get_argument_value("enabled");
  StringView period = command.get_argument_value("period");
  StringView duration = command.get_argument_value("duration");

  if (enabled.is_empty()) {
    enabled = "true";
  }

  if (period.is_empty() || period.to_integer() == 0) {
    period = "100";
  }

  command.print_options(printer());

  if (enabled != "true") {
    SL_PRINTER_TRACE("debug trace not enabled");
    return is_success();
  } else {
    if (is_connection_ready() == false) {
      SL_PRINTER_TRACE(
        "debug trace not enabled because a device is not connected");
      return printer().close_fail();
    }
  }
  SlPrinter::Output printer_output_guard(printer());

  set_update_period(period.to_integer() * 1_milliseconds);
  set_duration(duration.to_integer() * 1_seconds);

  start_timers();
  return is_success();
}

bool DebugTrace::analyze(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(analyze, "parses fault messages emitted by the system.")
      + GROUP_ARG_OPT(
        application_app,
        string,
        <none>,
        "path to the application elf file.")
      + GROUP_ARG_OPT(
        fault,
        string,
        <none>,
        "specific fault to analyze received over the serial debug port.")
      + GROUP_ARG_OPT(os, string, <none>, "path to the OS elf file."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView fault = command.get_argument_value("fault");
  const StringView os = command.get_argument_value("os");
  const StringView application = command.get_argument_value("application");

  command.print_options(printer());

  if (fault.is_empty()) {
    if (!is_connection_ready()) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());

  const StringView effective_os = [&](){
    if( os.is_empty() ){
      return workspace_settings().get_debug_analyze_os();
    }
    workspace_settings().set_debug_analyze_os(os);
    return os;
  }();


  File os_elf_file(effective_os);
  if (is_error() && effective_os.is_empty() == false) {
    APP_RETURN_ASSIGN_ERROR("failed to load os elf file " + effective_os);
  } else {
    reset_error();
  }

  const StringView effective_application = [&](){
    if( application.is_empty() ){
      return workspace_settings().get_debug_analyze_app();
    }
    workspace_settings().set_debug_analyze_app(application);
    return application;
  }();

  File app_elf_file(effective_application);
  if (is_error() && effective_application.is_empty() == false) {
    APP_RETURN_ASSIGN_ERROR(
      "failed to load application elf file " + effective_application);
  } else {
    reset_error();
  }

  auto os_symbol_list = swd::Elf(os_elf_file).get_symbol_list();
  reset_error();
  auto application_symbol_list = swd::Elf(app_elf_file).get_symbol_list();
  reset_error();

  if (fault.is_empty() == false) {
    auto fault_tokens = fault.split(":");

    StringView task_string = fault_tokens.at(0);
    StringView fault_string = fault_tokens.at(1);
    StringView address_string = fault_tokens.at(2);
    StringView program_counter_string = fault_tokens.at(3);
    StringView caller_string = fault_tokens.at(4);
    StringView handler_program_address_string = fault_tokens.at(5);
    StringView handler_caller_string = fault_tokens.at(6);

    task_string.pop_front(1);
    fault_string.pop_front(1);
    address_string.pop_front(1);
    program_counter_string.pop_front(2);
    caller_string.pop_front(1);
    handler_program_address_string.pop_front(2);
    handler_caller_string.pop_front(2);

    printer().open_object("fault");
    {
      u32 tmp_address;
      printer().key(
        "taskId",
        String().format(
          "%d",
          task_string.to_unsigned_long(StringView::Base::hexadecimal)));

      tmp_address = fault_string.to_unsigned_long(String::Base::hexadecimal);
      printer().key("fault", String().format("0x%08x", tmp_address));

      switch (tmp_address) {
      case 0x0:
        fault_string = "None";
        break;
      case 0x1:
        fault_string = "Memory Stacking";
        break;
      case 0x2:
        fault_string = "Memory Unstacking";
        break;
      case 0x3:
        fault_string = "Memory Access";
        break;
      case 0x4:
        fault_string = "Memory Execution";
        break;
      case 0x5:
        fault_string = "Bus Stacking";
        break;
      case 0x6:
        fault_string = "Bus Unstacking";
        break;
      case 0x7:
        fault_string = "Bus Imprecise";
        break;
      case 0x8:
        fault_string = "Bus Precise";
        break;
      case 0x9:
        fault_string = "Bus Instruction";
        break;
      case 0xA:
        fault_string = "Usage Divide by Zero";
        break;
      case 0xB:
        fault_string = "Usage Unaligned";
        break;
      case 0xC:
        fault_string = "Usage No coprocessor";
        break;
      case 0xD:
        fault_string = "Usage Invalid program counter";
        break;
      case 0xE:
        fault_string = "Usage Invalid State";
        break;
      case 0xF:
        fault_string = "Usage Undefined Instruction";
        break;
      case 0x10:
        fault_string = "Hard Vector Table";
        break;
      case 0x11:
        fault_string = "Hard Unhandled Interrupt";
        break;
      case 0x12:
        fault_string = "Memory Floating Point Lazy";
        break;
      case 0x13:
        fault_string = "Hard Unknown";
        break;
      case 0x14:
        fault_string = "Bus Unknown";
        break;
      case 0x15:
        fault_string = "Memory Unknown";
        break;
      case 0x16:
        fault_string = "Usage Unknown";
        break;
      case 0x17:
        fault_string = "Watch Dog Timeout";
        break;
      default:
        fault_string = "Unknown";
        break;
      }

      printer().key("faultInfo", fault_string);

      tmp_address
        = address_string.to_unsigned_long(StringView::Base::hexadecimal);
      printer().key("address", String().format("0x%08x", tmp_address));

      printer().key(
        "addressFunction",
        get_address_function(
          os_symbol_list,
          application_symbol_list,
          tmp_address));

      tmp_address = program_counter_string.to_unsigned_long(
        StringView::Base::hexadecimal);
      printer().key("programCounter", String().format("0x%08x", tmp_address));

      printer().key(
        "addressFunction",
        get_address_function(
          os_symbol_list,
          application_symbol_list,
          tmp_address));

      tmp_address
        = caller_string.to_unsigned_long(StringView::Base::hexadecimal);
      printer().key("caller", String().format("0x%08x", tmp_address));

      printer().key(
        "callerFunction",
        get_address_function(
          os_symbol_list,
          application_symbol_list,
          tmp_address));

      tmp_address = handler_program_address_string.to_unsigned_long(
        StringView::Base::hexadecimal);
      printer().key(
        "handlerProgramCounter",
        String().format("0x%08x", tmp_address));

      printer().key(
        "handlerProgramCounterFunction",
        get_address_function(
          os_symbol_list,
          application_symbol_list,
          tmp_address));

      tmp_address
        = handler_caller_string.to_unsigned_long(StringView::Base::hexadecimal);
      printer().key("handlerCaller", String().format("0x%08x", tmp_address));

      printer().key(
        "handlerCallerFunction",
        get_address_function(
          os_symbol_list,
          application_symbol_list,
          tmp_address));

      printer().close_object();
    }
  } else {

    sos::TraceEvent trace_event;
    hal::FrameBuffer::Info info;
    hal::FrameBuffer trace_fifo(
      connection()->info().trace_name(),
      fs::OpenMode::read_only().set_non_blocking(),
      connection()->driver());

    info = trace_fifo.get_info();

    if (info.frame_count_ready() > 0) {
      printer().start_table(var::StringViewList(
        {"index",
         "timestamp",
         "id",
         "thread",
         "pid",
         "programAddress",
         "message",
         "function"}));
      for (u32 i = 0; i < info.frame_count_ready(); i++) {
        trace_fifo.read(var::View(trace_event.event()));

        var::String id;
        switch (trace_event.id()) {
        case LINK_POSIX_TRACE_FATAL:
          id = "fatal";
          break;
        case LINK_POSIX_TRACE_CRITICAL:
          id = "critical";
          break;
        case LINK_POSIX_TRACE_WARNING:
          id = "warning";
          break;
        case LINK_POSIX_TRACE_MESSAGE:
          id = "message";
          break;
        case LINK_POSIX_TRACE_ERROR:
          id = "error";
          break;
        default:
          id = "other";
          break;
        }

        auto address_function = get_address_function(
          os_symbol_list,
          application_symbol_list,
          trace_event.program_address());

        chrono::ClockTime clock_time;
        clock_time = trace_event.timestamp();

        printer().append_table_row(StringViewList(
          {NumberString(i, "[%d]"),
           NumberString().format(
             F32U ".%06ld",
             clock_time.seconds(),
             clock_time.nanoseconds() / 1000UL),
           id,
           NumberString().format("%d", trace_event.thread_id()),
           NumberString().format("%d", trace_event.pid()),
           NumberString().format("0x%lX", trace_event.program_address()),
           trace_event.message(),
           address_function}));

        push_queue(JsonDocument().stringify(
          JsonObject()
            .insert("index", JsonInteger(i))
            .insert("timestampSeconds", JsonInteger(clock_time.seconds()))
            .insert("id", JsonInteger(trace_event.thread_id()))
            .insert("pid", JsonInteger(trace_event.pid()))
            .insert(
              "programAddress",
              JsonInteger(trace_event.program_address()))
            .insert("message", JsonString(trace_event.message()))
            .insert("function", JsonString(address_function.string_view()))));
      }
      printer().finish_table();
    }
  }

  return is_success();
}

var::PathString DebugTrace::get_address_function(
  const swd::Elf::SymbolList &os_symbol_list,
  const swd::Elf::SymbolList &application_symbol_list,
  u32 address) const {

  // symbol will be the closest value that is greater than
  // or equal to the symbol value

  u32 difference = 0xffffffff;
  PathString result;

  for (const auto &symbol : os_symbol_list) {
    const u32 value = symbol.value() & ~0x1;

    if (
      symbol.type() == swd::Elf::SymbolType::function && (address >= value)
      && (address - value < difference)) {
      difference = address - value;
      result = symbol.name();
    }
  }

  for (const auto &symbol : application_symbol_list) {
    const u32 value = symbol.value() & ~0x1;
    if (
      symbol.type() == swd::Elf::SymbolType::function && (address >= value)
      && (address - value < difference)) {
      difference = address - value;
      result = symbol.name();
    }
  }

  if (difference > 1024 * 1024) {
    return PathString("<no symbol>");
  }

#if !defined __win32
  int status;
  PathString demangled;
  size_t size = demangled.capacity();
  abi::__cxa_demangle(result.cstring(), demangled.data(), &size, &status);
  if (demangled.is_empty()) {
    return result;
  }
  return demangled;
#else
  return result;
#endif
}
