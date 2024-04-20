// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "SlPrinter.hpp"
#include <var/String.hpp>
#include <var/Vector.hpp>

class Command {
public:
  class Details {
  public:
    Details() {}

    static bool ascending_argument(const Details &a, const Details &b) {
      return a.argument() < b.argument();
    }

    bool operator==(const Details &a) const {
      if (argument() == a.argument()) {
        return true;
      }
      if (shortcut() == a.argument()) {
        return true;
      }
      return false;
    }

    bool is_required() const { return required() == "req"; }
    bool is_number() const { return type() == "int"; }
    bool is_string() const { return type() == "string"; }

    const Details &set_effective_value(const var::StringView value) const {
      m_effective_value = value.to_string();
      return *this;
    }

    const var::StringView effective_value() const {
      return m_effective_value.string_view();
    }

  private:
    API_ACCESS_STRING(Details, type);
    API_ACCESS_STRING(Details, required);
    API_ACCESS_STRING(Details, argument);
    API_ACCESS_STRING(Details, shortcut);
    API_ACCESS_STRING(Details, value);
    API_ACCESS_STRING(Details, default_value);
    API_ACCESS_STRING(Details, description);
    API_ACCESS_STRING(Details, example);
    mutable var::String m_effective_value;
  };

  class Group {
  public:
    Group(const var::StringView name) { m_name = name; }
    const var::StringView name() const { return m_name; }

  private:
    var::StringView m_name;
  };

  Command(Group command_group, const var::StringView command_string);
  Command(json::JsonObject command_object);

  Command &append(const var::String &command_string) {
    if (m_command_string.is_empty() == false) {
      m_command_string += ",";
    }
    m_command_string += command_string;
    return *this;
  }

  static var::String stringify(const json::JsonObject object);

  var::String usage() const;
  void show_help(SlPrinter &printer) const;

  bool is_valid() const { return m_is_valid; }
  const var::String &name() const { return m_name; }

  const var::String &error_message() const { return m_error_message; }

  const var::StringView
  get_argument_value(const var::StringView argument) const;

  void print_options(SlPrinter &printer) const;

  bool is_valid(const Command &reference, SlPrinter &printer) const;

  const var::String &group() const { return m_group; }

  static var::StringView get_validate_syntax_argument() {
    return "validateSyntax";
  }

  static bool is_admin() {
#if defined __admin_release
    return true;
#else
    return false;
#endif
  }

private:
  var::String m_command_string;
  var::String m_group;
  var::String m_name;
  var::String m_error_message;
  var::Vector<Details> m_details_list;
  Details m_empty_details;
  bool m_is_valid;
  mutable const Command *m_reference_command = nullptr;
  API_ACCESS_BOOL(Command, validate, false);

  const var::Vector<Details> &details_list() const { return m_details_list; }

  bool is_argument_present(
    const var::StringView name,
    const var::StringView shortcut) const;
  void parse_command_string();
  const var::StringView
  search_argument_value(const var::StringView argument) const;
  const Details &lookup_argument_value(const var::StringView input) const;
};

class ReferenceCommand : public Command {
public:
};

#endif // COMMAND_HPP
