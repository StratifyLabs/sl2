// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <sys.hpp>
#include <var.hpp>

#include "Command.hpp"

Command::Command(Group command_group, const var::StringView command_string) {
  m_group = command_group.name().to_string();
  m_command_string = command_string.to_string();
  parse_command_string();
}

var::String Command::stringify(const json::JsonObject object) {
  var::String result;
  result
    = object.at("group").to_string() + "." + object.at("command").to_string();

  json::JsonArray argument_array = object.at("arguments");
  const size_t argument_count = argument_array.count();

  if (argument_count == 0) {
    return result;
  }

  result += ":";
  for (size_t i = 0; i < argument_count; i++) {
    json::JsonObject argument_object = argument_array.at(i);
    result += argument_object.at("name").to_string_view() + "="
              + argument_object.at("value").to_string_view();
    result += ",";
  }
  result.pop_back();
  return result;
}

void Command::parse_command_string() {
  m_is_valid = true;
  // don't parse between ||

  Tokenizer command_name_and_arguments(
    m_command_string,
    Tokenizer::Construct()
      .set_delimiters(":")
      .set_ignore_between("|'\"")
      .set_maximum_delimiter_count(1));

  if (command_name_and_arguments.count() > 2) {
    m_is_valid = false;
    // set error_message
    // too many equal signs
    m_error_message = "There should only be one ':' in each group command";
    return;
  }

  m_name = command_name_and_arguments.at(0).to_string();

  if (command_name_and_arguments.count() == 2) {
    Tokenizer arguments_tokens(
      command_name_and_arguments.at(1),
      Tokenizer::Construct().set_delimiters(",").set_ignore_between("|'\""));

    bool is_reference_command = false;
    for (u32 i = 0; i < arguments_tokens.count(); i++) {

      Tokenizer arguments_values_and_descriptions(
        arguments_tokens.at(i),
        Tokenizer::Construct().set_delimiters("|"));

      Tokenizer arguments_values(
        arguments_values_and_descriptions.at(0),
        Tokenizer::Construct()
          .set_delimiters("=")
          .set_ignore_between("'\"")
          .set_maximum_delimiter_count(1));

      Details detail;

      if (arguments_values_and_descriptions.count() > 1) {
        detail.set_description(
          arguments_values_and_descriptions.at(1).to_string());
      }

      if (arguments_values_and_descriptions.count() > 3) {
        detail.set_example(
          arguments_values_and_descriptions.at(3).to_string());
      }

      if (!is_reference_command) {
        if (arguments_values.at(0) == "description") {
          is_reference_command = true;
        }
      }

      if (arguments_values.at(0) == get_validate_syntax_argument()) {
        set_validate(true);
      } else {

        if (arguments_values.count()) {
          // pushes the argument
          if (is_reference_command) {

            StringViewList a = arguments_values.at(0).split("_");

            detail.set_argument(a.at(0).to_string());
            detail.set_shortcut(
              a.count() > 1 ? a.at(1).to_string() : StringView());
          } else {
            detail.set_argument(arguments_values.at(0).to_string());
          }
        }

        if (arguments_values.count() == 2) {
          // pushes the value
          String value = arguments_values.at(1).to_string();

          // check for enclosing ' and strip them
          if (value.length() && value.at(0) == '\'') {
            // strip the first and last '
            value.pop_front();
            if (value.length() > 0 && value.at(value.length() - 1) == '\'') {
              value.pop_back();
            }
          }

          if (is_reference_command) {
            StringViewList v = value.split("_");
            detail.set_required(v.at(0).to_string());
            detail.set_type(
              v.count() > 1 ? v.at(1).to_string() : StringView());
            detail.set_default_value(
              v.count() > 2 ? v.at(2).to_string() : StringView());
          } else {
            detail.set_value(value);
          }
        } else if (arguments_values.count() == 1) {
          detail.set_value(String("true"));
        } else {
          detail.set_value(String("error"));
        }

        m_details_list.push_back(detail);
      }
    }

    m_details_list.sort(Details::ascending_argument);
  }
}

bool Command::is_valid(const Command &reference, SlPrinter &printer) const {
  bool result = true;
  m_reference_command = &reference;

  const auto help = get_argument_value("help");
  String error_message;

  if (m_reference_command == nullptr) {
    printer.fatal("reference command is not provided");
    exit(1);
  }

  if (help == "true") {
    m_reference_command->show_help(printer);
    return false;
  }

  // are all arguments recognized?
  for (u32 i = 0; i < details_list().count(); i++) {
    bool is_recognized = false;
    for (u32 j = 0; j < m_reference_command->details_list().count(); j++) {
      if (
        m_reference_command->details_list().at(j).argument() != "description") {
        if (
						(details_list().at(i).argument() == m_reference_command->details_list().at(j).argument())
						|| (details_list().at(i).argument() == m_reference_command->details_list().at(j).shortcut())) {
          is_recognized = true;
        }
      }
    }

    if (!is_recognized) {
      error_message = "`" + details_list().at(i).argument()
                      + "` is not a recognized argument";
      result = false;
    }
  }

  // are any required arguments missing
  for (u32 i = 0; i < m_reference_command->details_list().count(); i++) {
    if (m_reference_command->details_list().at(i).required() == "req") {
      if (!is_argument_present(
            m_reference_command->details_list().at(i).argument(),
            m_reference_command->details_list().at(i).shortcut())) {
        error_message = "required argument `"
                        + m_reference_command->details_list().at(i).argument()
                        + "` is missing";
        result = false;
      }
    }
  }

  // check for duplicates
  for (u32 i = 0; i < m_details_list.count() && result; i++) {
    for (u32 j = 0; j < m_details_list.count() && result; j++) {
      if (i != j) {
        if (
          m_details_list.at(i).argument() == m_details_list.at(j).argument()) {
          error_message = "argument `" + m_details_list.at(i).argument()
                          + "` is duplicated";
          result = false;
        }
      }
    }
  }

  if (!result) {
    printer.open_output("syntax");
    printer.error(error_message);
    printer.troubleshoot(
      "Usage is \n```\nsl " + m_reference_command->usage() + "\n```\n");
    printer.troubleshoot(
      "Use `sl --help=" + m_reference_command->group() + "."
      + m_reference_command->name() + "` for more details.");
  }

  if (is_validate()) {
    return false;
  }

  return result;
}

const StringView
Command::search_argument_value(const var::StringView argument) const {
  for (u32 i = 0; i < details_list().count(); i++) {
    if (argument == details_list().at(i).argument()) {
      return details_list().at(i).value();
    }
  }
  return StringView();
}

const Command::Details &
Command::lookup_argument_value(const var::StringView input_argument) const {

  if ((input_argument == "help") || (input_argument == "readme")) {
    for (const Details &details : details_list()) {
      if ((details.argument() == input_argument)) {
        return details.set_effective_value(details.value());
      }
    }
  }

  for (const Details &reference_details : m_reference_command->details_list()) {
    if (input_argument == reference_details.argument()) {
      for (const Details &details : details_list()) {
        if (
          (details.argument() == reference_details.shortcut())
          || (details.argument() == reference_details.argument())) {
          if (details.value().is_empty()) {
            return reference_details.set_effective_value(
              details.default_value());
          } else {
            return reference_details.set_effective_value(details.value());
          }
        }
      }
      return reference_details.set_effective_value(
        reference_details.default_value());
    }
  }
  return m_empty_details;
}

const var::StringView
Command::get_argument_value(const var::StringView argument) const {

  if (m_reference_command == nullptr) {
    printf(
      "fatal error -- can't call `Command::get_argument_value()` before "
      "`Command::is_valid()`\n");
    exit(1);
  }

  const Details &details = lookup_argument_value(argument);
  if (
    !details.effective_value().is_empty()
    && (details.effective_value().at(0) == '<')) {
    // if default value is `<value>` then just make it blank
    details.set_effective_value(StringView());
  }

  // if type is number -- convert from hex if needed
  if (details.is_number()) {
    if (details.effective_value().find("0x") == 0) {
      details.set_effective_value(NumberString(
        details.effective_value().to_unsigned_long(String::Base::hexadecimal)));
    }
  }

  return details.effective_value();
}

void Command::print_options(SlPrinter &printer) const {
  // print all values in reference -- use defaults or present values
  PrinterOptions po(printer);
  for (const Details &details : m_reference_command->details_list()) {
    // is argument from reference specified in this command
    if (details.argument() != "description") {
      const StringView value = get_argument_value(details.argument());
      printer.key(
        details.argument(),
        value.is_empty() ? details.default_value() : value);
    }
  }
}

bool Command::is_argument_present(
  const var::StringView name,
  const var::StringView shortcut) const {
  for (u32 i = 0; i < details_list().count(); i++) {
    if (
      (name == details_list().at(i).argument())
      || (shortcut == details_list().at(i).argument())) {
      return true;
    }
  }
  return false;
}

void Command::show_help(SlPrinter &printer) const {
  printer.open_paragraph();
  printer << String("Usage:\n```sh\nsl " + usage() + "\n```").string_view();
  printer.close_paragraph().newline();
  printer.open_paragraph();
  printer << String("Help with this command:\n```sh\nsl --help=" + group() + "." + name() + "\n```").string_view();
  printer.close_paragraph().newline();
  bool is_arguments_open = false;

  for (const Details &details : details_list()) {
    if (is_admin() && details.argument() == "description") {
      printf(
        "%s",
        String(
          "GROUP_ARG_DESC(" + name() + ",\"" + details.description() + "\")\n")
          .cstring());
    }
  }

  String example_required_arguments;
  for (const Details &details : details_list()) {
    if (details.argument() != "description") {

      const u32 pos = details.example().find(details.argument());
      const StringView abbreviated_example
        = details.example().get_substring_at_position(pos);

      String argument = details.argument().to_string();
      if (!details.shortcut().is_empty()) {
        (argument += "_" + details.shortcut());
      }

      if (details.is_required() == false) {
        if (is_admin()) {

          printf(
            "GROUP_ARG_OPT(%s,%s,%s,\"%s\")\n",
            argument.cstring(),
            details.type().data(),
            details.default_value().data(),
            details.description().data());
        }
      } else {
        String default_value = details.default_value().is_empty()
                                 ? String("<") + details.argument() + ">"
                                 : details.default_value().to_string();
        if (!example_required_arguments.is_empty()) {
          example_required_arguments += ",";
        }
        example_required_arguments
          += details.argument() + "=" + details.default_value();
        if (is_admin()) {
          printf(
            "GROUP_ARG_REQ(%s,%s,%s,\"%s\")\n",
            argument.cstring(),
            details.type().data(),
            default_value.cstring(),
            details.description().data());
        }
      }
    }
  }

  for (const Details &details : details_list()) {
    if (details.argument() == "description") {
      printer.open_header("Description");
      printer.open_paragraph();
      String description = "The `" + group() + "." + name() + "` command "
                           + details.description();
      printer
        << (details.description().is_empty() ? StringView("not available")
                                             : description.string_view());
      printer.close_paragraph();
      printer.close_header();
    }
  }

  // reference is command:argument_arg=req/opt_type_default|description
  for (const Details &details : details_list()) {

    if (details.argument() != "description") {

      if (is_arguments_open == false) {
        printer << "\n";
        printer.open_header("arguments");
        is_arguments_open = true;
      }

      printer << "\n";
      printer.open_header(details.argument());
      printer.open_list(printer::MarkdownPrinter::ListType::unordered);
      {
        if (!details.shortcut().is_empty()) {
          printer.key("shortcut", "`" + details.shortcut() + "`");
        }

        String d = details.description().to_string();
        if (d.length()) {
          d.at(0) = tolower(d.at(0));
          if (d.back() != '.') {
            d.push_back('.');
          }
        }

        d(String::Replace().set_old_string("*").set_new_string("`"));
        d(String::Replace().set_old_string("'").set_new_string("`"));

        printer.key("description", d.is_empty() ? String("not available") : d);

        if (!details.example().is_empty()) {

          String example_this;
          if (!details.is_required()) {
            example_this = details.argument() + "=" + details.default_value();
          }

          if (!example_required_arguments.is_empty())

            printer.key(
              "example",
              "`" + group() + "." + name() + ":" + example_required_arguments
                + (example_required_arguments.is_empty() || example_this.is_empty() ? "" : ",")
                + example_this + "`");
        }

        if (details.argument() != "<none>") {

          printer.key_bool("required", details.is_required());
          printer.key(
            "type",
            details.type().is_empty() ? String("unknown")
                                      : String(details.type()));
          if (!details.is_required()) {
            printer.key("default", "`" + details.default_value() + "`");
          }
        }

        printer.close_list();
        printer.close_header();
      }
    }
  }

  if (is_arguments_open) {
    printer.close_header();
  }
}

String Command::usage() const {
  // reference is command:argument_arg=req/opt_type_default|description
  String result;
  result.reserve(4096);

  result += group() + ".";
  result += name();

  if (details_list().count() > 0) {
    if (details_list().count() > 1) {
      result += ":";
    } else {
      return result;
    }
  }

  for (u32 i = 1; i < details_list().count(); i++) {

    bool is_required = (details_list().at(i).required() == "req");

    if (!is_required) {
      result += "[";
    }
    if (i > 1) {
      result += ",";
    }
    result += details_list().at(i).argument() + "=<"
              + details_list().at(i).type() + ">";
    if (!is_required) {
      result += "]";
    }
    result += "\n\t";
  }

  return result;
}
