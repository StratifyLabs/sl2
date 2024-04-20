// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef __SL_PRINTER_HPP
#define __SL_PRINTER_HPP

#include <chrono/ClockTimer.hpp>
#include <fs/FileSystem.hpp>
#include <json/Json.hpp>
#include <json/JsonDocument.hpp>
#include <printer/JsonPrinter.hpp>
#include <printer/MarkdownPrinter.hpp>
#include <printer/YamlPrinter.hpp>
#include <sos/Appfs.hpp>
#include <sos/Link.hpp>
#include <sos/Sys.hpp>
#include <sos/TaskManager.hpp>
#include <var/Queue.hpp>
#include <var/String.hpp>

// used for printing to the output
class PrinterCallback {
public:
  PrinterCallback(const var::String &report) : m_report(report) {}
  using callback_t = void (*)(void *, const var::StringView);

  const var::String report() const { return m_report; }

private:
  API_AF(PrinterCallback, callback_t, handle_input, nullptr);
  API_AF(PrinterCallback, void *, context, nullptr);
  const var::String &m_report;
};

class SlJsonPrinter : public printer::JsonPrinter {
public:
  SlJsonPrinter(PrinterCallback &callback) : m_callback(callback) {}
  void interface_print_final(const var::StringView value) override {
    fwrite(value.data(), value.length(), 1, stdout);
    fflush(stdout);
    m_callback.handle_input()(m_callback.context(), value);
  }

  ~SlJsonPrinter() {
    if ((m_callback.report().length()) && (m_callback.report().at(0) == '[')) {
      api::ErrorGuard error_guard;
      API_RESET_ERROR();
      json::JsonDocument document;
      document.from_string(m_callback.report());
      if (document.is_error()) {
        printer::Printer p;
        p << document.error();
        exit(1);
      }
    }
  }

private:
  PrinterCallback &m_callback;
};

class SlYamlPrinter : public printer::YamlPrinter {
public:
  SlYamlPrinter(PrinterCallback &callback) : m_callback(callback) {}
  void interface_print_final(const var::StringView value) override {
    fwrite(value.data(), value.length(), 1, stdout);
    fflush(stdout);
    m_callback.handle_input()(m_callback.context(), value);
  }

private:
  PrinterCallback &m_callback;
};

class SlMarkdownPrinter : public printer::MarkdownPrinter {
public:
  SlMarkdownPrinter(PrinterCallback &callback) : m_callback(callback) {}
  void interface_print_final(const var::StringView value) override {
    fwrite(value.data(), value.length(), 1, stdout);
    fflush(stdout);
    m_callback.handle_input()(m_callback.context(), value);
  }

private:
  PrinterCallback &m_callback;
};

class SlPrinter : public printer::Printer {
public:
  SlPrinter();
  ~SlPrinter();

  void open_external_output();
  void close_external_output();

  void open_terminal_output();
  void close_terminal_output();

  void open_command(const var::StringView command);
  void close_command();

  void open_options(
    printer::Printer::Level level = printer::Printer::Level::message);
  void close_options();
  void option_key(const var::StringView key, const var::StringView value);

  void option_key_bool(const var::StringView key, bool value) {
    return option_key(key, value ? "true" : "false");
  }

  void open_connecting();
  bool close_connecting_success(const var::StringView serial_number);
  bool close_connecting_fail();

  void open_login();
  bool close_login_success(const var::StringView uid);
  bool close_login_fail();

  void
  open_output(const var::StringView name = "", const var::StringView id = "");

  class Output {
  public:
    Output(
      SlPrinter &printer,
      const var::StringView name = "output",
      const var::StringView id = "")
      : m_printer(printer) {
      printer.open_output(name, id);
    }

    ~Output() { m_printer.close_output(); }

  private:
    SlPrinter &m_printer;
  };

  void close_output();
  void close_output(const var::StringView key, const var::StringView value);
  bool close_success() {
    close_output("result", "success");
    return true;
  }
  bool close_fail() {
    close_output("result", "fail");
    return false;
  }

  void output_key(const var::StringView key, const var::StringView value);

  void transfer_info(
    const var::StringView source_file,
    const chrono::ClockTimer &transfer_timer,
    link_transport_mdriver_t *driver);

  void transfer_info(
    const sos::Link::Path &link_path,
    const chrono::ClockTimer &transfer_timer) {
    transfer_info(link_path.path(), transfer_timer, link_path.driver());
  }

  void print_os_install_options();

  SlPrinter &set_verbose_level(const var::StringView level) {
    markdown().set_verbose_level(level);
    yaml().set_verbose_level(markdown().verbose_level());
    json().set_verbose_level(markdown().verbose_level());
    return *this;
  }

  printer::Printer::Level verbose_level() const {
    return markdown().verbose_level();
  }
  bool is_bash() const { return markdown().is_bash(); }
  SlPrinter &set_bash(bool value = true) {
    markdown().set_bash(value);
    yaml().set_bash(value);
    return *this;
  }

  SlPrinter &set_json();
  bool is_json() const { return m_is_json; }

  SlPrinter &set_vanilla();
  SlPrinter &set_insert_codefences();
  bool is_vanilla() const { return m_is_vanilla; }


  void set_suppressed(bool value = true) { m_is_suppressed = value; }

  bool is_suppressed() const { return m_is_suppressed; }

  printer::Printer &connecting() { return active_printer(); }

  printer::Printer &output() {
    if (is_json()) {
      return json();
    }
    return yaml();
  }

  SlPrinter &open_object(
    const var::StringView name,
    printer::Printer::Level level = printer::Printer::Level::info) {
    if (m_is_output) {
      m_output_open_count++;
    }
    if (m_is_json) {
      json().open_object(name, level);
    } else if (m_is_insert_codefences) {
      yaml().open_object(name, level);
    } else {
      yaml().open_array(name, level);
    }
    return *this;
  }

  SlPrinter &close_object() {
    if (m_is_json) {
      json().close_object();
    } else if (m_is_insert_codefences) {
      yaml().close_object();
    } else {
      yaml().close_array();
    }

    if (m_is_output && m_output_open_count) {
      m_output_open_count--;
    }

    return *this;
  }

  SlPrinter &open_array(
    const var::StringView name,
    printer::Printer::Level level = printer::Printer::Level::info) {
    if (is_json()) {
      json().open_array(name, level);
    } else {
      yaml().open_array(name, level);
    }
    return *this;
  }

  SlPrinter &close_array() {
    if (is_json()) {
      json().close_array();
    } else {
      yaml().close_array();
    }
    return *this;
  }

  SlPrinter &horizontal_line() {
    markdown().horizontal_line();
    return *this;
  }

  SlPrinter &open_paragraph(
    printer::Printer::Level level = printer::Printer::Level::info) {
    markdown().open_paragraph(level);
    return *this;
  }

  SlPrinter &close_paragraph() {
    markdown().close_paragraph();
    return *this;
  }

  SlPrinter &open_list(
    printer::MarkdownPrinter::ListType type,
    printer::Printer::Level level = printer::Printer::Level::info) {
    markdown().open_list(type, level);
    return *this;
  }

  SlPrinter &close_list() {
    markdown().close_list();
    return *this;
  }

  SlPrinter &open_header(
    const var::StringView value,
    printer::Printer::Level level = printer::Printer::Level::info) {
    if (is_json()) {
      json().open_object(value, level);
    } else {
      markdown().open_header(value, level);
    }
    return *this;
  }

  SlPrinter &close_header() {
    if (is_json()) {
      json().close_object();
    } else {
      markdown().close_header();
    }
    return *this;
  }

  SlPrinter &open_code(
    const var::StringView value = var::String(),
    printer::Printer::Level level = printer::Printer::Level::info) {
    markdown().open_code(value, level);
    return *this;
  }

  SlPrinter &close_code() {
    markdown().close_code();
    return *this;
  }

  SlPrinter &troubleshoot(const var::StringView value);
  SlPrinter &troubleshoot_bootloader_connected();

  SlPrinter &debug(const var::StringView value);
  SlPrinter &message(const var::StringView value);
  SlPrinter &info(const var::StringView value);
  SlPrinter &warning(const var::StringView value);
  SlPrinter &syntax_error(const var::StringView value);
  SlPrinter &error(const var::StringView value);
  SlPrinter &fatal(const var::StringView value);

  SlPrinter &key(const var::StringView key, const var::StringView value) {
    active_printer().key(key, value);
    return *this;
  }

  SlPrinter &key_bool(const var::StringView key, bool value) {
    active_printer().key_bool(key, value);
    return *this;
  }

  SlPrinter &key(const var::StringView key, const json::JsonValue &a) {
    active_printer().object(key, a);
    return *this;
  }

  template <class T>
  SlPrinter &object(
    const var::StringView key,
    const T &a,
    printer::Printer::Level level = printer::Printer::Level::info) {
    active_printer().object<T>(key, a, level);
    return *this;
  }

  SlPrinter &operator<<(const json::JsonObject &value) {
    active_printer() << value;
    return *this;
  }

  SlPrinter &operator<<(const fs::PathList &value) {
    active_printer() << value;
    return *this;
  }

  SlPrinter &operator<<(const json::JsonArray &value) {
    active_printer() << value;
    return *this;
  }

  SlPrinter &operator<<(const sos::Sys::Info &value) {
    active_printer() << value;
    return *this;
  }

  SlPrinter &operator<<(const sos::TaskManager::Info &value) {
    active_printer() << value;
    return *this;
  }

  SlPrinter &operator<<(const sos::Appfs::Info &value) {
    active_printer() << value;
    return *this;
  }

  SlPrinter &operator<<(const sos::TraceEvent &value) {
    active_printer() << value;
    return *this;
  }

  SlPrinter &operator<<(const var::Vector<var::String> &value) {
    active_printer() << value;
    return *this;
  }

  SlPrinter &operator<<(const var::StringView value) {
    active_printer() << value;
    return *this;
  }

  SlPrinter &operator<<(const char *value) {
    active_printer() << var::String(value);
    return *this;
  }

  SlPrinter &operator<<(const fs::FileInfo &value) {
    active_printer() << value;
    return *this;
  }

  // const var::StringView  progress_key() const { return yaml().progress_key();
  // }
  const var::StringView progress_key() {
    return active_printer().progress_key();
  }
  const var::StringView progress_key() const {
    return active_printer().progress_key();
  }
  const api::ProgressCallback *progress_callback() const {
    return active_printer().progress_callback();
  }
  api::ProgressCallback::IsAbort update_progress(int progress, int total) {
    if (is_json()) {
      return json().update_progress(progress, total);
    }
    return yaml().update_progress(progress, total);
  }

  SlPrinter &start_table(const var::StringViewList &header);
  SlPrinter &append_table_row(const var::StringViewList &row);
  SlPrinter &
  finish_table(printer::Printer::Level level = printer::Printer::Level::info);

  SlPrinter &insert_newline(
    printer::Printer::Level level = printer::Printer::Level::info) {
    if (
      (markdown().verbose_level() >= level) && (&active_printer() != &json())) {
      markdown() << printer::MarkdownPrinter::Directive::insert_newline;
    }
    return *this;
  }

  const printer::Printer &active_printer() const {
    if (m_is_output || m_is_options || m_is_yaml || m_is_json) {
      if (m_is_json) {
        return json();
      }
      return yaml();
    }
    return markdown();
  }

  printer::Printer &active_printer() {
    if (m_is_output || m_is_options || m_is_yaml || m_is_json) {
      if (m_is_json) {
        return json();
      }
      return yaml();
    }
    return markdown();
  }

  printer::MarkdownPrinter &markdown() { return m_markdown_printer; }

  const printer::MarkdownPrinter &markdown() const {
    return m_markdown_printer;
  }

private:
  API_AS(SlPrinter, report);
  API_AC(SlPrinter, var::Queue<var::String>, queue);

  bool m_is_vanilla;
  bool m_is_insert_codefences;
  bool m_is_yaml;
  bool m_is_json;
  var::String m_current_command;
  var::StringList m_debug_list;
  var::StringList m_troubleshoot_list;
  bool m_is_suppressed;
  bool m_is_options;
  bool m_is_output;
  int m_output_open_count;
  PrinterCallback m_printer_callback;
  SlJsonPrinter m_json_printer;
  SlYamlPrinter m_yaml_printer;
  SlMarkdownPrinter m_markdown_printer;
  var::Vector<var::StringList> m_table;

  static void process_output(void *context, const var::StringView output) {
    reinterpret_cast<SlPrinter *>(context)->process_output(output);
  }

  void process_output(const var::StringView output) {
    m_report += output;
    m_queue.push(output.to_string());
  }

  printer::YamlPrinter &yaml() { return m_yaml_printer; }
  const printer::JsonPrinter &json() const { return m_json_printer; }

  printer::JsonPrinter &json() { return m_json_printer; }

  const printer::YamlPrinter &yaml() const { return m_yaml_printer; }

  void open_printer_section(
    const var::StringView name,
    const var::StringView id,
    printer::Printer::Level level);

  void
  close_printer_section(const var::StringView key, const var::StringView value);

  void show_debug_and_troubleshoot();
  void close_yaml_printer();
  void external_separator_begin(const var::StringView key);
  void external_separator_end(const var::StringView key);
};

class PrinterOptions {
public:
  explicit PrinterOptions(SlPrinter &printer) : m_printer(printer) {
    m_printer.open_options();
  }

  ~PrinterOptions() { m_printer.close_options(); }

private:
  SlPrinter &m_printer;
};

class PrinterCode {
public:
  explicit PrinterCode(
    SlPrinter &printer,
    const var::StringView code_info,
    printer::Printer::Level level = printer::Printer::Level::info)
    : m_printer(printer) {
    m_printer.open_code(code_info, level);
  }

  ~PrinterCode() { m_printer.close_code(); }

private:
  SlPrinter &m_printer;
};

class PrinterHeader {
public:
  PrinterHeader(SlPrinter &printer, const var::String value)
    : m_printer(printer) {
    m_printer.open_header(value);
  }

  ~PrinterHeader() { m_printer.close_header(); }

private:
  SlPrinter &m_printer;
};

#define SL_PRINTER_TRACE(msg) PRINTER_TRACE(printer().output(), msg)
#define SL_PRINTER_TRACE_PERFORMANCE()

#endif // __SL_PRINTER_HPP
