#include "../utilities/OperatingSystem.hpp"
#include "../utilities/Packager.hpp"
#include <fs.hpp>
#include <var.hpp>

#include "Admin.hpp"

Admin::Admin() : Group("admin", "admin") {}

bool Admin::execute_command_at(u32 list_offset, const Command &command) {

  switch (list_offset) {
  case command_upload:
    return upload(command);
  case command_import:
    return import(command);
  }
  return false;
}

var::StringViewList Admin::get_command_list() const {
  StringViewList list = {
    "upload",
    "import",
  };

  API_ASSERT(list.count() == command_total);

  return list;
}

bool Admin::upload(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  // install the project -- app or BSP
  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(upload, "is currently for internal use only.")
      + GROUP_ARG_OPT(sl, bool, false, "upload the latest version of sl")
      + GROUP_ARG_OPT(
        preview,
        bool,
        false,
        "used with `sl` to indicate this is a preview release")
      + GROUP_ARG_OPT(clean, bool, false, "delete the temporary folder")
      + GROUP_ARG_OPT(filter, string, <none>, "file pattern to filter")
      + GROUP_ARG_OPT(dryrun, bool, false, "don't upload just do a dry run"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto sl = command.get_argument_value("sl");
  const auto filter = command.get_argument_value("filter");
  const auto clean = command.get_argument_value("clean");
  const auto dry_run = command.get_argument_value("dryrun");
  const auto preview = command.get_argument_value("preview");

  command.print_options(printer());

  if (!is_cloud_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  if (sl != "true") {
    APP_RETURN_ASSIGN_ERROR("Must specify either `sl`");
  }

  if (sl == "true") {
    if (
      cloud_service().cloud().credentials().get_uid()
      != "9cQyOTk3fZXOuMtPyf5j9X4H4b83") {
      APP_RETURN_ASSIGN_ERROR("you don't have permissions to run this");
    }

    SL_PRINTER_TRACE("sl path is " + OperatingSystem::get_path_to_sl());

    Packager()
      .set_keep_temporary(clean != "true")
      .publish(Packager::PublishOptions()
                 .set_preview(preview == "true")
                 .set_name("sl")
                 .set_version(VERSION)
                 .set_dryrun(dry_run == "true")
                 .set_source_path(OperatingSystem::get_path_to_sl()));

    if (is_error()) {
      APP_RETURN_ASSIGN_ERROR("failed to upload `sl`");
    }
  }


  return is_success();
}

bool Admin::import(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  // install the project -- app or BSP
  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(import, "is currently for internal use only.")
      + GROUP_ARG_OPT(
        symbols,
        string,
        false,
        "the path to the CRT .S file of symbols")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        false,
        "file path destination for the import"));

  if (!command.is_valid(reference, printer())) {

    return printer().close_fail();
  }

  StringView symbols = command.get_argument_value("symbols");
  StringView destination = command.get_argument_value("destination");

  command.print_options(printer());

  if (!is_cloud_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object("import");

  if (!symbols.is_empty()) {

    if (destination.is_empty()) {
      APP_RETURN_ASSIGN_ERROR(
        "must specify a `destination` when importing symbols");
    }

    const auto parent = Path::parent_directory(destination);
    if (!FileSystem().directory_exists(parent)) {
      APP_RETURN_ASSIGN_ERROR(parent | " directory not found");
    }

    const DataFile symbols_file = DataFile().write(File(symbols)).move();

    if (is_error()) {
      printer().error("failed to load a CRT file from " + symbols);
      return printer().close_fail();
    }

    String line;

    const DataFile output_file
      = DataFile()
          .write("//Auto generated file. Do not modifiy\n\n")
          .write("#include <var.hpp>\n\n")
          .move();

    while (!(line = symbols_file.get_line()).is_empty()) {

      if (line.string_view().find("_signature") != String::npos) {

        u32 entry_count = 0;
        output_file.write(String().format(
          "String sos_crt_symbols_get_signature(){ return String(\"%s\"); "
          "}\n\n",
          NameString(line.string_view().split(" ").at(4)).cstring()));

        output_file.write("const char * sos_symbols[] = {\n");

        while (!(line = symbols_file.get_line()).is_empty()) {
          auto item_list = line.string_view().split(" ");

          if (
            item_list.count() > 1 && item_list.at(0).length()
            && item_list.at(0).at(0) != '/') {
            SL_PRINTER_TRACE("write symbol " + item_list.at(1));
            output_file.write(String().format(
              "\t\"%s\",\n",

              NameString(item_list.at(1).pop_front().pop_back()).cstring()));
            entry_count++;
          }
        }

        output_file.write("\tnullptr\n};\n\n")
          .write(GeneralString()
                   .format(
                     "u32 sos_crt_symbols_get_count(){ return %d; }\n\n",
                     entry_count)
                   .string_view())
          .write(GeneralString()
                   .format(
                     "String sos_crt_symbols_get_entry(u32 value){ if( value < "
                     "sos_crt_symbols_get_count() ){ return "
                     "String(sos_symbols[value]); "
                     "} "
                     "return String(); }\n\n",
                     entry_count)
                   .string_view());
      }
    }

    File(File::IsOverwrite::yes, destination).write(output_file.seek(0));

    if (is_error()) {
      APP_RETURN_ASSIGN_ERROR("failed to save file at " + destination);
    }
  }

  return is_success();
}
