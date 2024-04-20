#ifndef SESSIONSETTINGS_HPP
#define SESSIONSETTINGS_HPP

#include <service.hpp>
#include <var.hpp>

class SessionSettings {
  API_ACCESS_STRING(SessionSettings, path);
  API_ACCESS_COMPOUND(SessionSettings, var::KeyString, serial_number);
  API_ACCESS_STRING(SessionSettings, driver);
  API_ACCESS_STRING(SessionSettings, invoked_as_path);
  API_ACCESS_STRING(SessionSettings, report_permissions);
  API_ACCESS_STRING(SessionSettings, report_team);
  API_ACCESS_COMPOUND(
    SessionSettings,
    var::StringList,
    tag_list); // for the report
  API_ACCESS_BOOL(SessionSettings, sdk_invoke_mismatch, false);
  API_ACCESS_BOOL(SessionSettings, request_update, false);
  API_ACCESS_BOOL(SessionSettings, cloud_initialized, false);
  API_ACCESS_BOOL(SessionSettings, msys, false);
  API_ACCESS_BOOL(SessionSettings, interactive, false);
  API_ACCESS_BOOL(SessionSettings, listen_mode, false);
  API_ACCESS_FUNDAMENTAL(SessionSettings, u16, listen_port, 3000);
  API_ACCESS_COMPOUND(
    SessionSettings,
    var::PathString,
    web_path); // for the report
  API_ACCESS_BOOL(SessionSettings, offline_mode, false);
  API_ACCESS_BOOL(SessionSettings, update_checked, false);
  API_ACCESS_BOOL(SessionSettings, upgrade_available, false);
  API_ACCESS_BOOL(SessionSettings, archive_history, false);
  API_ACCESS_BOOL(SessionSettings, interrupted, false);
  API_ACCESS_STRING(SessionSettings, call_graph_path);
  API_ACCESS_STRING(SessionSettings, terminal_report);
  API_ACCESS_COMPOUND(SessionSettings, var::StringList, group_list);

public:
  SessionSettings() {}

  SessionSettings &add_tag(const Document::Tag &tag) {
    m_tag_list.push_back(String(tag.get_tag().string_view()));
    return *this;
  }

  var::String get_report_name() const {
    var::String result;
    for (const auto &item : group_list()) {
      result += item + ", ";
    }
    result.pop_back().pop_back();
    return result;
  }

  SessionSettings &add_project_tag(const Project &project) {
    if (project.get_document_id().is_empty() == false) {
      add_tag(Document::Tag().set_key("project").set_value(
        project.get_document_id()));
    }
    return *this;
  }

  SessionSettings &add_project_tag(const var::StringView id) {
    if (id.is_empty() == false) {
      add_tag(Document::Tag().set_key("project").set_value(id));
   }
    return *this;
  }

  SessionSettings &add_team_tag(const var::StringView team) {
    if (team.is_empty() == false) {
      add_tag(Document::Tag().set_key("team").set_value(team));
    }
    return *this;
  }

  SessionSettings &add_thing_tag(const var::StringView serial_number) {
    if (serial_number.is_empty() == false) {
      add_tag(Document::Tag().set_key("thing").set_value(serial_number));
    }
    return *this;
  }

  SessionSettings &add_thing_tag(const Thing &thing) {
    if (
      thing.get_system_information().get_serial_number().is_empty() == false) {
      add_tag(Document::Tag().set_key("thing").set_value(
        thing.get_system_information().get_serial_number()));
    }
    return *this;
  }
};

#endif // SESSIONSETTINGS_HPP
