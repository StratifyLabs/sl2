#include <hal/Flash.hpp>
#include <var.hpp>

#include "Flash.hpp"

FlashGroup::FlashGroup() : Connector("flash", "flash") {}

var::StringViewList FlashGroup::get_command_list() const {

  StringViewList list = {"ping", "write", "erase"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool FlashGroup::execute_command_at(u32 list_offset, const Command &command) {

  switch (list_offset) {
  case command_ping:
    return ping(command);
  }
  return false;
}

bool FlashGroup::ping(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(ping, "oings information about the flash device")
      + GROUP_ARG_REQ(
        path_source,
        string,
        <path to flash device>,
        "path to the flash device."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView path = command.get_argument_value("path");

  command.print_options(printer());

  if (is_connection_ready(IsBootloaderOk::no) == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  sos::Link::Path source_path(path, connection()->driver());

  if (source_path.prefix() == sos::Link::Path::host_prefix()) {
    APP_RETURN_ASSIGN_ERROR("path must be a device path (use `device@`");
  }

  hal::Flash flash_device(
    source_path.path(),
    OpenMode::read_write(),
    connection()->driver());

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("failed to open device " | path);
  }

  auto page_info_list = flash_device.get_page_info();
  auto size = flash_device.get_size();

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("failed to get data from the flash device");
  }

  printer()
    .output()
    .key("version", NumberString(flash_device.get_version(), "0x%04x"))
    .key("size", NumberString(size));

  printer::Printer::Object pages_object(printer().output(), "pages");
  for (const auto page : page_info_list) {
    printer().output().object(NumberString(page.page()), page);
  }

  return is_success();
}

bool FlashGroup::write(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(ping, "writes data to the flash device")
      + GROUP_ARG_REQ(
        destination_dest,
        string,
        <path to flash device>,
        "path to the flash device where the file will be written.")
    + GROUP_ARG_REQ(
        path_source,
        string,
        <path to flash device>,
        "path to the source file to write to the flash.")
      + GROUP_ARG_OPT(
        signkey_signKey,
        string,
        <none>,
        "key to use to sign the file written to the flash.")
      + GROUP_ARG_OPT(
        signkeypassword_signKeyPassword,
        string,
        <null>,
        "password to use to access the private sign key.")
    );

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView path = command.get_argument_value("path");

  command.print_options(printer());

  if (is_connection_ready(IsBootloaderOk::no) == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  sos::Link::Path source_path(path, connection()->driver());

  if (source_path.prefix() == sos::Link::Path::host_prefix()) {
    APP_RETURN_ASSIGN_ERROR("path must be a device path (use `device@`");
  }

  hal::Flash flash_device(
    source_path.path(),
    OpenMode::read_write(),
    connection()->driver());

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("failed to open device " | path);
  }

  auto version = flash_device.get_version();

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("failed to get the device driver version");
  }

  auto page_info_list = flash_device.get_page_info();
  auto size = flash_device.get_size();

  //erase the pages that need to be erased -- based on address and file size





  printer()
    .output()
    .key("version", NumberString(version, "0x%04x"))
    .key("size", NumberString(size));

  printer::Printer::Object pages_object(printer().output(), "pages");
  for (const auto page : page_info_list) {
    printer().output().object(NumberString(page.page()), page);
  }

  return is_success();
}

bool FlashGroup::erase(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(ping, "oings information about the flash device")
      + GROUP_ARG_REQ(
        path_source,
        string,
        <path to flash device>,
        "path to the flash devic."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView path = command.get_argument_value("path");

  command.print_options(printer());

  if (is_connection_ready(IsBootloaderOk::no) == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  sos::Link::Path source_path(path, connection()->driver());

  if (source_path.prefix() == sos::Link::Path::host_prefix()) {
    APP_RETURN_ASSIGN_ERROR("path must be a device path (use `device@`");
  }

  hal::Flash flash_device(
    source_path.path(),
    OpenMode::read_write(),
    connection()->driver());

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("failed to open device " | path);
  }

  auto page_info_list = flash_device.get_page_info();
  auto size = flash_device.get_size();

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("failed to get data from the flash device");
  }

  printer()
    .output()
    .key("version", NumberString(flash_device.get_version(), "0x%04x"))
    .key("size", NumberString(size));

  printer::Printer::Object pages_object(printer().output(), "pages");
  for (const auto page : page_info_list) {
    printer().output().object(NumberString(page.page()), page);
  }

  return is_success();
}
