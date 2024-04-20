#include <printer.hpp>
#include <sos/fs/drive_assetfs.h>
#include <var.hpp>

#include "Mcu.hpp"

Mcu::Mcu() : Group("mcu", "mcu") {}

var::StringViewList Mcu::get_command_list() const {

  StringViewList list = {"process", "parse", "bundle"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool Mcu::execute_command_at(u32 list_offset, const Command &command) {

  switch (list_offset) {
  case command_process:
    return process(command);
  case command_bundle:
    return bundle(command);
  case command_parse:
    return parse(command);
  }
  return false;
}

bool Mcu::process(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(process, "process mcu configuration data.")
      + GROUP_ARG_REQ(
        architecture_arch,
        string,
        <stm32>,
        "architecture to process.")
      + GROUP_ARG_OPT(source_path, string, <none>, "path to input file.")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <none>,
        "path to destination output folder.")
      + GROUP_ARG_OPT(
        insert,
        bool,
        false,
        "use with `checksum` and `arch=lpc` to insert the required checksum to "
        "an LPC binary file. (if no destination is provided, the source is "
        "modified).")
      + GROUP_ARG_OPT(
        checksum,
        bool,
        false,
        "verify the checksum of the source file (use with `arch=lpc`"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView architecture = command.get_argument_value("architecture");

  command.print_options(printer());

  SlPrinter::Output printer_output_guard(printer());

  if (architecture == "stm32") {
    return process_stm32(command);
  }

  if (architecture == "lpc") {
    return process_lpc(command);
  }

  return is_success();
}

bool Mcu::parse(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(process, "process mcu configuration data.")
      + GROUP_ARG_REQ(
        architecture_arch,
        string,
        <stm32>,
        "architecture to parse.")
      + GROUP_ARG_OPT(source_path, string, <none>, "location of input file.")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <none>,
        "location of output file.")
      + GROUP_ARG_OPT(interrupts, bool, <false>, "parse the interrupts file.")
      + GROUP_ARG_OPT(pins, bool, <false>, "parse the pins file."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView architecture = command.get_argument_value("architecture");
  const StringView source = command.get_argument_value("source");
  const StringView destination = command.get_argument_value("destination");
  const StringView interrupts = command.get_argument_value("interrupts");

  command.print_options(printer());

  SlPrinter::Output printer_output_guard(printer());

  if (architecture == "stm32" && interrupts == "true") {
    return parse_stm32_interrupts(
      ParseStm32Interrupts().set_source(source).set_destination(destination));
  }

  return is_success();
}

bool Mcu::parse_stm32_interrupts(const ParseStm32Interrupts &options) {

  SL_PRINTER_TRACE("parsing STM32 interrrupts");
  if (FileSystem().exists(options.source()) == false) {
    APP_RETURN_ASSIGN_ERROR(options.destination() | " does not exist");
  }

  GeneralString line;

  struct Pair {
    String name;
    String value;
  };

  var::Vector<Pair> pair_list;

  {
    SL_PRINTER_TRACE("opening  " | options.source());
    File input_file(options.source());

    while ((line = input_file.get_line()).is_empty() == false) {

      if (
        line.string_view().find("_IRQn") != String::npos
        && line.string_view().find("}") == String::npos) {

        String condensed_line = String(line.string_view());

        condensed_line
          .replace(String::Replace().set_old_string(" ").set_new_string(""))
          .replace(String::Replace().set_old_string("\r").set_new_string(""))
          .replace(String::Replace().set_old_string("\n").set_new_string(""))
          .replace(String::Replace().set_old_string("\t").set_new_string(""));

        const auto tokens = condensed_line.string_view().split("=,/");

        if (tokens.count() > 3) {
          Pair pair;
          pair.name = String(tokens.at(0));
          pair.value = String(tokens.at(1));
          SL_PRINTER_TRACE(
            "name is " | tokens.at(0) | " value is " | tokens.at(1));
          pair_list.push_back({String(tokens.at(0)), String(tokens.at(1))});
        }
      }

      if (line.string_view().find("IRQn_Type") != String::npos) {
        break;
      }
    }
  }

  SL_PRINTER_TRACE("creating  " | options.destination());
  File output_file(File::IsOverwrite::yes, options.destination());

  for (const auto &pair : pair_list) {
    s32 value = pair.value.to_integer();
    auto name = pair.name;

    name.to_lower().erase(
      String::Erase().set_position(name.string_view().find("_irqn")));

    if (value >= 0) {
      output_file.write(String().format(
        "_DECLARE_ISR(%s); //" F32D "\n",
        name.cstring(),
        value));
    }
  }

  int count = INT_MAX;
  for (const auto &pair : pair_list) {
    s32 value = pair.value.to_integer();
    String name = pair.name;

    name.to_lower().erase(
      String::Erase().set_position(name.string_view().find("_irqn")));

    while (count < value - 1) {
      output_file.write(
        String().format("mcu_core_default_isr, // " F32D "\n", ++count));
    }

    if (value >= 0) {
      output_file.write(
        String().format("_ISR(%s), //" F32D "\n", name.cstring(), value));
      count = value;
    }
  }

  return true;
}

bool Mcu::bundle(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(bundle, "create a binary blob of assets.")
      + GROUP_ARG_REQ(
        source_path,
        string,
        <none>,
        "source file/directory to convert.")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <none>,
        "destination file/directory.")
      + GROUP_ARG_OPT(
        access,
        string,
        0444,
        "use a read-only combo (default is 0444)")
      + GROUP_ARG_OPT(owner, string, root, "specify `user` or `root`."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  command.print_options(printer());

  SlPrinter::Output printer_output_guard(printer());

  return is_success();
}

bool Mcu::process_lpc(const Command &command) {
  Printer::Object po(printer().output(), "lpc");
  const auto source = command.get_argument_value("source");
  const auto destination = command.get_argument_value("destination");
  const auto insert = command.get_argument_value("insert");
  const auto checksum = command.get_argument_value("checksum");

  if (!FileSystem().exists(source)) {
    APP_RETURN_ASSIGN_ERROR(source | " could not be found");
  }

  if (!destination.is_empty()) {
    const auto parent = Path::parent_directory(destination);
    if (!FileSystem().directory_exists(parent)) {
      APP_RETURN_ASSIGN_ERROR("Cannot create the output file " | destination);
    }

    if (
      FileSystem().exists(destination)
      && !FileSystem().get_info(destination).is_file()) {
      APP_RETURN_ASSIGN_ERROR(
        "Cannot overwrite the output file " | destination);
    }
  }

  const DataFile source_file = DataFile(File(source));

  if (checksum == "true") {
    auto source_memory = [&]() {
      var::Array<u32, 8> result;
      source_file.seek(0).read(result);
      return result;
    }();

    auto get_sum = [&](int count) {
      API_ASSERT(count <= source_memory.count());
      u32 result = 0;
      for (auto i : api::Index(count)) {
        result += source_memory.at(i);
      }
      return result;
    };

    const auto is_valid = get_sum(8) == 0;

    if (insert == "true") {

      File destination_file = File(
        File::IsOverwrite::yes,
        destination.is_empty() ? source : destination);

      // insert the checksum
      if (!is_valid) {
        const auto sum = get_sum(7);
        SL_PRINTER_TRACE("intermediate sum is " | NumberString(sum, "0x%08x"));
        source_memory.back() = 0x00 - get_sum(7);
      }

      {
        Printer::FlagScope flag_scope(printer().output());
        printer()
          .output()
          .set_flags(Printer::Flags::width_32 | Printer::Flags::hex)
          .object("array", source_memory, Printer::Level::debug);
      }

      printer().output().key(
        "checkValue",
        NumberString(source_memory.back(), "0x%08x"));

      View(source_file.data()).copy(source_memory);

      destination_file.write(source_file.seek(0));

    } else {
      // verify the checksum

      {
        Printer::FlagScope flag_scope(printer().output());
        printer()
          .output()
          .set_flags(Printer::Flags::width_32 | Printer::Flags::hex)
          .object("array", source_memory, Printer::Level::debug);
      }

      printer().output().key(
        "checkValue",
        NumberString(source_memory.back(), "0x%08x"));

      printer().output().key_bool("valid", is_valid);
      return is_valid;
    }
  } else {
    APP_RETURN_ASSIGN_ERROR(
      "Nothing to do. Specify `checksum=true` to verify the checksum.");
  }

  return true;
}

bool Mcu::process_stm32(const Command &command) {

  Printer::Object po(printer().output(), "stm32");
  const StringView source = command.get_argument_value("source");
  const StringView destination = command.get_argument_value("destination");

  if (destination.is_empty()) {
    APP_RETURN_ASSIGN_ERROR("`destination` must be specified");
  }

  if (FileSystem().directory_exists(destination) == false) {
    APP_RETURN_ASSIGN_ERROR(destination + " folder does not exist");
  }

  const PathString loader_path
    = Path::parent_directory(source) / "../bin/FlashLoader";

  JsonObject stm32_config = JsonDocument().load_xml(File(source));

  API_RETURN_VALUE_IF_ERROR(false);

  printer().object("configuration", stm32_config);

  JsonArray device_array
    = stm32_config.at("Root").to_object().at("Device").to_array();

  for (u32 i = 0; i < device_array.count(); i++) {
    const StringView device_id
      = device_array.at(i).to_object().at("DeviceID").to_string_view();
    Printer::Object device_printer_object(printer().output(), device_id);
    const PathString loader_elf = loader_path / device_id & ".stldr";

    if (FileSystem().exists(loader_elf)) {
      printer().key("elf", loader_path / device_id & ".stldr");
      const auto json_file_path = destination / device_id & ".json";
      JsonDocument().save(
        device_array.at(i),
        File(File::IsOverwrite::yes, json_file_path));

      // create an abbreviated JSON file
      const JsonObject input_object = JsonDocument().load(File(json_file_path));
      const auto json_brief_file_path = destination / device_id & "_brief.json";
      JsonDocument().save(
        JsonObject()
          .insert("name", input_object.at("Name"))
          .insert("type", input_object.at("Type"))
          .insert("series", input_object.at("Series"))
          .insert("cpu", input_object.at("CPU"))
          .insert("deviceId", input_object.at("DeviceID"))
          .insert("vendor", input_object.at("Vendor")),
        File(File::IsOverwrite::yes, json_brief_file_path));

      File(File::IsOverwrite::yes, destination / device_id & ".elf")
        .write(File(loader_elf));
    }
  }

  return is_success();
}
