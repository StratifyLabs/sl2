// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <cloud.hpp>
#include <fs.hpp>
#include <printer.hpp>
#include <sos.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "SlPrinter.hpp"

SlPrinter::SlPrinter()
  : m_printer_callback(m_report), m_json_printer(m_printer_callback),
    m_yaml_printer(m_printer_callback), m_markdown_printer(m_printer_callback) {
  m_is_options = false;
  m_is_output = false;
  m_is_suppressed = false;
  m_is_vanilla = false;
  m_is_insert_codefences = false;
  m_is_yaml = false;
  m_is_json = false;
  m_output_open_count = 0;
  m_printer_callback.set_handle_input(process_output).set_context(this);
}

SlPrinter::~SlPrinter() {
  if (is_json()) {
    // json().close_array();
    printf("]\n");
    m_report += "]\n";
  }
}

SlPrinter &SlPrinter::set_json() {
  m_is_vanilla = true;
  m_is_json = true;
  json().enable_flags(Printer::Flags::simple_progress);
  cloud::CloudObject::set_default_printer(json());
  printf("[");
  m_report = "[";
  return *this;
}

SlPrinter &SlPrinter::set_vanilla() {
  m_is_vanilla = true;
  markdown().enable_flags(Printer::Flags::simple_progress);
  yaml().enable_flags(Printer::Flags::simple_progress);
  markdown().disable_flags(
    Printer::Flags::bold_values | Printer::Flags::green_values
    | Printer::Flags::red_values | Printer::Flags::yellow_warnings
    | Printer::Flags::red_errors);
  return *this;
}

SlPrinter &SlPrinter::set_insert_codefences() {
  m_is_insert_codefences = true;
  yaml().disable_flags(
    Printer::Flags::red_errors | Printer::Flags::yellow_warnings);
  markdown().disable_flags(
    Printer::Flags::red_errors | Printer::Flags::yellow_warnings);
  return set_vanilla();
}

void SlPrinter::open_external_output() {
  external_separator_begin("external.begin");
}

void SlPrinter::close_external_output() {
  external_separator_end("external.end");
}

void SlPrinter::open_terminal_output() {
  external_separator_begin("terminal.begin");
}

void SlPrinter::close_terminal_output() {
  external_separator_end("terminal.end");
}

void SlPrinter::external_separator_begin(const var::StringView key) {
  if (is_json()) {
    json().open_object(key);
  } else {
    open_header(key);
    if (m_is_insert_codefences) {
      open_code(key);
    }
  }
}

void SlPrinter::external_separator_end(const var::StringView key) {
  if (is_json()) {
    json().close_object();
  } else {
    if (m_is_insert_codefences) {
      close_code();
    }
    markdown() << MarkdownPrinter::Directive::insert_newline;
    close_header();
    open_header(key);
    close_header();
    markdown() << MarkdownPrinter::Directive::insert_newline;
  }
}

void SlPrinter::open_command(const var::StringView command) {
  if (!is_suppressed()) {
    if (is_json()) {
      json().open_object(command);
      json().key("command", command);

    } else {
      Printer::FlagGuard fg(markdown());
      if (!is_vanilla()) {
        markdown().enable_flags(
          Printer::Flags::bold_keys | Printer::Flags::yellow_values);
      }
      open_header(command, Printer::Level::warning);
      m_current_command = command.to_string();
    }
  }
}

void SlPrinter::close_command() {
  m_current_command.clear();
  if (is_json()) {
    json().close_object();
  } else {
    close_header();
  }
}

void SlPrinter::open_printer_section(
  const var::StringView name,
  const var::StringView id,
  Printer::Level level) {

  if (is_json()) {
    json().open_object(name, level);
  } else {
    if (!is_suppressed()) {
      m_yaml_printer.set_verbose_level(markdown().verbose_level());
      m_yaml_printer.set_top_verbose_level(level);
      // if( !is_vanilla() ){ enable_flags(Printer::Flags::bold_values); }
      if (level <= markdown().verbose_level()) {
        markdown() << MarkdownPrinter::Directive::insert_newline;
      }
      open_header(name, level);
      // if( !is_vanilla() ){ disable_flags(Printer::Flags::bold_values); }

      if (m_is_insert_codefences) {
        open_code("yaml", level);
        yaml().open_array(name, level);
      }
    }
  }
}

void SlPrinter::close_printer_section(
  const var::StringView key,
  const var::StringView value) {

  if (is_json()) {
    json().close_object();
  }

  if (!is_suppressed() && !key.is_empty()) {
    if (is_json()) {
      json().key(key, value);
    } else {
      Printer::FlagGuard fg(yaml());
      if (!is_vanilla()) {
        if (value == "fail") {
          yaml().enable_flags(
            Printer::Flags::red_values | Printer::Flags::cyan_keys);
        } else {
          yaml().enable_flags(
            Printer::Flags::green_values | Printer::Flags::cyan_keys);
        }
      }
      yaml().key(key, value);
    }
  }

  if (!is_json() && !is_suppressed()) {
    if (m_is_insert_codefences) {
      yaml().close_array();
      close_code();
    }
  }

  if (key == "result") {
    close_header(); // close output header
  }

  if (!is_json() && !is_suppressed()) {
    insert_newline();
    show_debug_and_troubleshoot();
    close_header();
  }
}

void SlPrinter::close_yaml_printer() {
  if (m_is_insert_codefences) {
    yaml().close_array();
    close_code();
  }
}

void SlPrinter::open_output(
  const var::StringView name,
  const var::StringView id) {
  var::String name_to_print = name.to_string();
  if (name_to_print.is_empty()) {
    name_to_print = "output";
  }
  m_is_output = true;
  m_output_open_count = 0;
  open_printer_section(name_to_print, id, Printer::Level::warning);
}

void SlPrinter::close_output() {
  if (is_error()) {

    switch (api::ExecutionContext::error().error_number()) {
    case 200:
      error(StringView(api::ExecutionContext::error().message()));
      break;
    case ENOENT:
      error(
        StringView("ENOENT -> ") & api::ExecutionContext::error().message());
      break;
    case EINVAL:
      error(
        StringView("EINVAL -> ") & api::ExecutionContext::error().message());
      break;
    case ENOSPC:
      error(
        StringView("ENOSPC -> ") & api::ExecutionContext::error().message());
      break;
    case EROFS:
      error(
        StringView("EROFS -> ") & api::ExecutionContext::error().message());
      break;
    case EPERM:
      error(
        StringView("EPERM -> ") & api::ExecutionContext::error().message());
      break;
    default:
      if (active_printer().verbose_level() == Printer::Level::trace) {
        markdown() << MarkdownPrinter::Directive::insert_newline;
        yaml().object(
          "unhandled internal error",
          api::ExecutionContext::error());
      } else {
        error(
          PathString().format(
            "Unhandled Error %d (use `--verbose=trace` for details) -> ",
            api::ExecutionContext::error().error_number())
          & api::ExecutionContext::error().message());
      }
    }

    close_output("result", "fail");
  } else {
    close_output("result", "success");
  }
}

void SlPrinter::close_output(
  const var::StringView key,
  const var::StringView value) {
  if (m_is_output) {
    while (m_output_open_count) {
      close_object();
    }
    m_is_output = false;
    close_printer_section(key, value);
  }
}

void SlPrinter::output_key(
  const var::StringView key,
  const var::StringView value) {
  active_printer().key(key, value);
}

void SlPrinter::open_connecting() { open_output("connecting"); }

bool SlPrinter::close_connecting_success(const var::StringView serial_number) {
  close_output("serialNumber", serial_number);
  return true;
}

bool SlPrinter::close_connecting_fail() {
  close_output("result", "fail");
  return false;
}

void SlPrinter::open_login() { open_output("cloud.login"); }

bool SlPrinter::close_login_success(const var::StringView uid) {
  close_output("uid", uid);
  return true;
}

bool SlPrinter::close_login_fail() {
  close_output("result", "fail");
  return false;
}

void SlPrinter::open_options(Printer::Level level) {
  m_is_options = true;
  open_printer_section("options", "", level);
}

void SlPrinter::close_options() {
  close_printer_section("", "");
  m_is_options = false;
}

void SlPrinter::option_key(
  const var::StringView key,
  const var::StringView value) {
  active_printer().key(key, value);
}

SlPrinter &SlPrinter::troubleshoot(const var::StringView value) {
  m_troubleshoot_list.push_back(value.to_string());
  return *this;
}

SlPrinter &SlPrinter::troubleshoot_bootloader_connected() {
  return troubleshoot(
    "If an os package is installed, try resetting the device. If an os package "
    "is not installed, use `sl "
    "cloud.install:id=<projectId>`. or `sl cloud.bootstrap:os` for supported "
    "devices.");
}

SlPrinter &SlPrinter::debug(const var::StringView value) {
  if (markdown().verbose_level() >= printer::Printer::Level::debug) {
    m_debug_list.push_back(value.to_string());
  }
  return *this;
}

SlPrinter &SlPrinter::message(const var::StringView value) {
  active_printer().message(value);
  return *this;
}

SlPrinter &SlPrinter::info(const var::StringView value) {
  active_printer().info(value);
  return *this;
}

SlPrinter &SlPrinter::warning(const var::StringView value) {
  Printer::FlagGuard fg(yaml());
  if (!is_vanilla()) {
    yaml().enable_flags(Printer::Flags::yellow_keys);
  }
  active_printer().warning(value);
  return *this;
}

SlPrinter &SlPrinter::syntax_error(const var::StringView value) {
  open_command("syntax.error");
  open_output();
  error(value);
  return *this;
}

SlPrinter &SlPrinter::error(const var::StringView value) {
  {
    Printer::FlagGuard fg(yaml());
    if (!is_vanilla()) {
      yaml().enable_flags(Printer::Flags::red_keys);
    }
    active_printer().error(value);
  }
  close_fail();
  return *this;
}

SlPrinter &SlPrinter::fatal(const var::StringView value) {
  active_printer().fatal(value);
  return *this;
}

void SlPrinter::show_debug_and_troubleshoot() {
  if (
    markdown().verbose_level() >= printer::Printer::Level::debug
    && m_debug_list.count()) {
    markdown() << MarkdownPrinter::Directive::insert_newline;
    markdown().open_blockquote(printer::Printer::Level::debug);
    for (auto const &message : m_debug_list) {
      markdown() << message;
    }
    markdown().close_blockquote();
  }
  m_debug_list.clear();

  if (m_troubleshoot_list.count() > 0) {
    markdown() << MarkdownPrinter::Directive::insert_newline;
    open_header("troubleshooting", Printer::Level::info);
    open_paragraph(Printer::Level::info);
    for (auto const &message : m_troubleshoot_list) {
      markdown() << message;
    }
    close_paragraph();
    close_header();
    m_troubleshoot_list.clear();
  }
}

SlPrinter &SlPrinter::start_table(const var::StringViewList &header) {
  m_table.clear();
  StringList list;
  for (auto value : header) {
    list.push_back(value.to_string());
  }
  markdown().open_pretty_table(list);
  m_table.push_back(list);
  if (!markdown().is_pretty_table_valid()) {
    error("internal error started table with empty header");
  }
  return *this;
}

SlPrinter &SlPrinter::append_table_row(const var::StringViewList &row) {

  if (!markdown().is_pretty_table_valid()) {
    error("internal error: can't create table without header");
    return *this;
  }

  StringList list;
  for (auto value : row) {
    list.push_back(value.to_string());
  }

  markdown().append_pretty_table_row(list);
  m_table.push_back(list);

  return *this;
}

SlPrinter &SlPrinter::finish_table(Printer::Level level) {
  if (m_is_insert_codefences || m_is_yaml || is_json()) {
    const var::Vector<var::String> &header = m_table.front();
    for (size_t row = 1; row < m_table.count(); row++) {
      active_printer().open_object(m_table.at(row).front(), level);
      for (size_t column = 1; column < m_table.at(row).count(); column++) {
        active_printer().key(header.at(column), m_table.at(row).at(column));
      }
      active_printer().close_object();
    }
  } else {
    insert_newline();
    markdown().close_pretty_table(Printer::Level::info);
  }
  return *this;
}

void SlPrinter::print_os_install_options() {
  troubleshoot("Use `sl os.install` to install from a local OS build");
  troubleshoot(
    "Use `sl cloud.install:id=<packageId>` install from a pre-built cloud "
    "image");
  troubleshoot(
    "Use `sl cloud.bootstrap:os` to install from a pre-built cloud image for "
    "supported mbed targets");
}

void SlPrinter::transfer_info(
  const var::StringView source_file_path,
  const chrono::ClockTimer &transfer_timer,
  link_transport_mdriver_t *source_file_driver) {
  API_RETURN_IF_ERROR();

  FileInfo file_info
    = Link::FileSystem(source_file_driver).get_info(source_file_path);

  open_object("transfer");
  active_printer().key("size", NumberString(file_info.size()));

  active_printer().key(
    "duration",
    NumberString(transfer_timer.milliseconds() * 1.0f / 1000.0f, "%0.3fs"));

  active_printer().key(
    "rate",
    NumberString(
      file_info.size() * 1.0f / transfer_timer.milliseconds() * 1000.0f,
      "%0.3fKB/s"));

  close_object();
}
