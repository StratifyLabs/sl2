#ifndef LOCALSETTINGS_HPP
#define LOCALSETTINGS_HPP

#include <chrono.hpp>
#include <json.hpp>

class SerialSettings : public json::JsonValue {
public:
  JSON_ACCESS_CONSTRUCT_OBJECT(SerialSettings);
  JSON_ACCESS_INTEGER(SerialSettings, baudrate);
  JSON_ACCESS_INTEGER_WITH_KEY(SerialSettings, stopBits, stop_bits);
  JSON_ACCESS_STRING(SerialSettings, parity);

private:
};

class ConnectionSettings : public json::JsonValue {
public:
  JSON_ACCESS_CONSTRUCT_OBJECT(ConnectionSettings);
  JSON_ACCESS_STRING(ConnectionSettings, path);
  JSON_ACCESS_STRING_WITH_KEY(ConnectionSettings, serialNumber, serial_number);
  JSON_ACCESS_BOOL(ConnectionSettings, exclusive);
  JSON_ACCESS_INTEGER(ConnectionSettings, retry);
  JSON_ACCESS_INTEGER(ConnectionSettings, delay);
};

class SdkProject : public json::JsonValue {
public:
  SdkProject() : json::JsonValue(json::JsonObject()) {
    set_architecture("arm");
    set_branch("master");
    set_tag("HEAD");
    set_make_options("-j8");
    set_cmake_options("-DSOS_ARCH_ARM_ALL=ON");
  }
  SdkProject(const json::JsonObject &object) : json::JsonValue(object) {

    if (object.at("buildable").is_valid() == false) {
      set_buildable();
    }

    // set defaults if they are empty
    if (get_tag().is_empty()) {
      set_tag("HEAD");
    }
    if (get_branch().is_empty()) {
      set_branch("master");
    }
    if (get_make_options().is_empty()) {
      set_make_options("-j12");
    }
    if (get_architecture().is_empty()) {
      set_architecture("arm");
    }
  }

  SdkProject(
    const var::String &name,
    const var::String &git_path,
    const var::String &branch,
    const var::String &tag) {
    set_name(name);
    set_url("https://" + git_path + "/" + name + ".git");
    if (tag.is_empty()) {
      set_tag("HEAD");
    } else {
      set_tag(tag);
    }

    if (branch.is_empty()) {
      set_branch("master");
    } else {
      set_branch(branch);
    }
  }

  JSON_ACCESS_STRING(SdkProject, name);
  JSON_ACCESS_STRING_WITH_KEY(SdkProject, cmo, cmake_options);
  JSON_ACCESS_STRING_WITH_KEY(SdkProject, mo, make_options);
  JSON_ACCESS_STRING(SdkProject, command);
  JSON_ACCESS_STRING_WITH_KEY(SdkProject, prebuild, pre_build);
  JSON_ACCESS_STRING_WITH_KEY(SdkProject, postbuild, post_build);
  JSON_ACCESS_STRING_WITH_KEY(SdkProject, target, build_target);
  JSON_ACCESS_STRING(SdkProject, url);
  JSON_ACCESS_STRING(SdkProject, architecture);
  JSON_ACCESS_STRING(SdkProject, tag);
  JSON_ACCESS_STRING(SdkProject, branch);
  JSON_ACCESS_BOOL(SdkProject, buildable);

private:
};

class ScriptCase;

class ScriptReference : public json::JsonValue {
public:
  JSON_ACCESS_CONSTRUCT_OBJECT(ScriptReference);

  bool is_specified() const { return get_name().is_empty() == false; }

  bool is_valid() const { return reference_case() != nullptr; }

  JSON_ACCESS_STRING(ScriptReference, name);
  JSON_ACCESS_STRING_WITH_KEY(ScriptReference, case, case_name);

private:
  API_ACCESS_FUNDAMENTAL(
    ScriptReference,
    ScriptCase *,
    reference_case,
    nullptr);
};

class ScriptCase : public json::JsonValue {
public:
  ScriptCase(const var::StringView name = "")
    : json::JsonValue(json::JsonObject()) {
    set_case_name(name);
  }

  ScriptCase(const json::JsonObject object)
    : json::JsonValue(object), m_reference(object.at("reference")) {
    if (get_architecture().is_empty()) {
      set_architecture("arm");
    }
    set_while_name(object.at("while").to_string());
    set_clean(object.at("clean").to_string());

    const json::JsonValue &duration_object = object.at("duration");
    if (duration_object.is_valid()) {
      set_duration(duration_object.to_integer() * 1_seconds);
    } else {
      set_duration(2_seconds);
    }

    if (object.at("result").is_valid()) {
      set_result(object.at("result").to_bool());
    } else {
      set_result();
    }

    if (object.at("fatal").is_valid()) {
      set_fatal(object.at("fatal").to_bool());
    } else {
      set_fatal();
    }

    if (object.at("assert").is_valid()) {
      set_assert(object.at("assert").to_bool());
    } else {
      set_assert();
    }

    if (get_count() == 0) {
      set_count(1);
    }
  }

  bool is_reference() const { return reference().is_valid(); }

  bool operator==(const ScriptCase &script_case) const {
    return get_case_name() == script_case.get_case_name();
  }

  JSON_ACCESS_STRING_WITH_KEY(ScriptCase, case, case_name);
  JSON_ACCESS_STRING_WITH_KEY(ScriptCase, target, build_target);
  JSON_ACCESS_STRING(ScriptCase, command);
  JSON_ACCESS_STRING(ScriptCase, arguments);
  JSON_ACCESS_STRING_WITH_KEY(ScriptCase, while, while_name);
  JSON_ACCESS_STRING(ScriptCase, architecture);
  JSON_ACCESS_STRING(ScriptCase, clean);
  JSON_ACCESS_INTEGER(ScriptCase, count);
  JSON_ACCESS_BOOL(ScriptCase, result);
  JSON_ACCESS_BOOL(ScriptCase, terminal);
  JSON_ACCESS_BOOL(ScriptCase, analyze);
  JSON_ACCESS_BOOL(ScriptCase, json);
  JSON_ACCESS_BOOL(ScriptCase, debug);
  JSON_ACCESS_BOOL_WITH_KEY(ScriptCase, showTerminal, show_terminal);
  JSON_ACCESS_BOOL(ScriptCase, fatal);
  JSON_ACCESS_BOOL(ScriptCase, assert);

private:
  API_ACCESS_COMPOUND(ScriptCase, ScriptReference, reference);
  API_ACCESS_COMPOUND(ScriptCase, chrono::MicroTime, duration);
};

#if defined NOT_BUILDING
class ScriptGroup : public json::JsonKeyValue {
public:
  ScriptGroup(
    const var::String &name,
    const JsonValue &value = json::JsonArray())
    : json::JsonKeyValue(name, value) {}

  JSON_ACCESS_KEY_VALUE_PAIR_ARRAY(ScriptGroup, ScriptCase, name, case_list);

  const ScriptCase &case_at(const var::StringView name) const {
    var::Vector<ScriptCase> case_list = get_case_list();
    for (const auto &script_case : case_list) {
      if (script_case.get_case_name() == name) {
        return script_case;
      }
    }
    return m_empty_script_case;
  }

  bool is_case_available(const var::StringView name) const {
    for (const auto &script_case : case_list()) {
      if (script_case.get_case_name() == name) {
        return true;
      }
    }
    return false;
  }

  static bool resolve_references(var::Vector<ScriptGroup> &group_list) {
    // look for references
    bool is_all_resolved = true;
    for (ScriptGroup &group : group_list) {
      for (ScriptCase &sc : group.case_list()) {
        if (sc.reference().is_specified()) {
          u32 group_offset
            = group_list.find(ScriptGroup(sc.reference().get_name()));
          if (group_offset < group_list.count()) {
            u32 case_offset
              = group_list.at(group_offset)
                  .case_list()
                  .find(ScriptCase(sc.reference().get_case_name()));
            if (case_offset < group_list.at(group_offset).case_list().count()) {
              sc.reference().set_reference_case(
                &group_list.at(group_offset).case_list().at(case_offset));
            } else {
              is_all_resolved = false;
            }
          } else {
            is_all_resolved = false;
          }
        }
      }
    }
    return is_all_resolved;
  }

  bool operator==(const ScriptGroup &group) const {
    return name() == group.name();
  }

private:
  static ScriptCase m_empty_script_case;
};

#endif
#endif // LOCALSETTINGS_HPP
