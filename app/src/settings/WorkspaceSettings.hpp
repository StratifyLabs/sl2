// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef WORKSPACE_SETTINGS_HPP
#define WORKSPACE_SETTINGS_HPP

#include <fs.hpp>
#include <json.hpp>
#include <var.hpp>

#include "settings/CloudSettings.hpp"
#include "settings/FilePathSettings.hpp"
#include "settings/GlobalSettings.hpp"
#include "settings/LocalSettings.hpp"

class AssetInfo : public json::JsonKeyValue {
public:
  explicit AssetInfo(
    const StringView name,
    const JsonValue &value = json::JsonObject())
    : json::JsonKeyValue(name, value) {}

  JSON_ACCESS_STRING(AssetInfo, destination);
  JSON_ACCESS_STRING_ARRAY_WITH_KEY(AssetInfo, sources, source_list);
};

class ShortcutInfo : public json::JsonKeyValue {
public:
  explicit ShortcutInfo(
    const StringView name,
    const JsonValue &value = json::JsonString())
    : json::JsonKeyValue(name, value) {}

  bool operator==(const ShortcutInfo &a) const { return key() == a.key(); }
};

using ShortcutInfoList = json::JsonKeyValueList<ShortcutInfo>;

class JobInfo : public json::JsonObject {
public:
  JobInfo() {}
  JobInfo(const JsonObject &object) : json::JsonObject(object) {}

  JSON_ACCESS_OBJECT_WITH_KEY(JobInfo, CloudJobInput, cloudJobInput, cloud_job);

  JSON_ACCESS_STRING_WITH_KEY(JobInfo, job, job_id);
  JSON_ACCESS_INTEGER(JobInfo, timeout);
};

class WorkspaceSettings : public json::JsonValue {
public:
  WorkspaceSettings();
  WorkspaceSettings(const json::JsonObject &object) : json::JsonValue(object) {
  }
  int initialize_workspace();

  bool is_file_present() const {
    return FileSystem().exists(FilePathSettings::workspace_settings_filename());
  }

  const json::JsonError &load_error() const { return m_load_error; }

  WorkspaceSettings &load() {
    json::JsonDocument json_document;
    *this = json_document
              .load(fs::File(FilePathSettings::workspace_settings_filename()))
              .to_object();
    m_load_error = json_document.error();
    API_RESET_ERROR();
    return *this;
  }

  const WorkspaceSettings &save() const {
    if (is_valid()) {
      json::JsonDocument().save(
        to_object(),
        fs::File(
          File::IsOverwrite::yes,
          FilePathSettings::workspace_settings_filename()));
    }
    return *this;
  }

  var::PathString sl_location() const;

  static var::StringView credentials_path() { return "sl_credentials.json"; }
  static int generate_configuration_header(const var::StringView path);

  static var::PathString current_workspace();

  static var::NameString workspace_name() {
    return fs::Path::name(current_workspace());
  }

  JSON_ACCESS_STRING_WITH_KEY(WorkspaceSettings, debugAnalyzeApp, debug_analyze_app);
  JSON_ACCESS_STRING_WITH_KEY(WorkspaceSettings, debugAnalyzeOs, debug_analyze_os);
  JSON_ACCESS_OBJECT_WITH_KEY(WorkspaceSettings, JobInfo, jobInfo, job_info);
  JSON_ACCESS_BOOL(WorkspaceSettings, json);
  JSON_ACCESS_BOOL(WorkspaceSettings, silent);
  JSON_ACCESS_BOOL(WorkspaceSettings, interative);
  JSON_ACCESS_INTEGER_WITH_KEY(
    WorkspaceSettings,
    creationTimestamp,
    creation_timestamp);
  JSON_ACCESS_BOOL_WITH_KEY(WorkspaceSettings, reportStatus, report_status);
  JSON_ACCESS_STRING_WITH_KEY(WorkspaceSettings, sdkDirectory, sdk_directory);
  JSON_ACCESS_STRING_WITH_KEY(WorkspaceSettings, slLocation, sl_location_value);
  JSON_ACCESS_STRING_WITH_KEY(WorkspaceSettings, signKey, sign_key);
  JSON_ACCESS_STRING_WITH_KEY(
    WorkspaceSettings,
    reportPermissions,
    report_permissions);

  JSON_ACCESS_STRING_WITH_KEY(WorkspaceSettings, reportTeam, report_team);
  JSON_ACCESS_STRING(WorkspaceSettings, verbose);
  JSON_ACCESS_STRING(WorkspaceSettings, version);
  JSON_ACCESS_OBJECT_WITH_KEY(
    WorkspaceSettings,
    SerialSettings,
    serialOptions,
    serial_settings);
  JSON_ACCESS_OBJECT_WITH_KEY(
    WorkspaceSettings,
    ConnectionSettings,
    lastConnection,
    last_connection);
  JSON_ACCESS_OBJECT_WITH_KEY(
    WorkspaceSettings,
    ConnectionSettings,
    preferredConnection,
    preferred_connection);
  JSON_ACCESS_OBJECT_LIST_WITH_KEY(
    WorkspaceSettings,
    ShortcutInfo,
    shortcuts,
    shortcut_list);
  JSON_ACCESS_OBJECT(WorkspaceSettings, json::JsonObject, script);
  JSON_ACCESS_ARRAY(WorkspaceSettings, SdkProject, sdk);
  JSON_ACCESS_STRING_ARRAY(WorkspaceSettings, blacklist);
  JSON_ACCESS_STRING(WorkspaceSettings, job);
  JSON_ACCESS_OBJECT_LIST_WITH_KEY(
    WorkspaceSettings,
    AssetInfo,
    assets,
    asset_list);

private:
  json::JsonError m_load_error;

  void set_defaults();
};

#endif // WORKSPACE_SETTINGS_HPP
