#ifndef CLOUDSETTINGS_HPP
#define CLOUDSETTINGS_HPP

#include <json/Json.hpp>
#include <service.hpp>
#include <var/String.hpp>

class CloudJobInput : public json::JsonObject {
public:
  CloudJobInput() {}
  CloudJobInput(const json::JsonObject &object) : json::JsonObject(object) {
    set_permissions("public");
  }

  JSON_ACCESS_STRING(CloudJobInput, command);
  JSON_ACCESS_STRING(CloudJobInput, team);
  JSON_ACCESS_STRING(CloudJobInput, permissions);
  JSON_ACCESS_BOOL(CloudJobInput, report);
};

class CloudJobOutput : public json::JsonObject {
public:
  CloudJobOutput() {}
  CloudJobOutput(const json::JsonObject &object) : json::JsonObject(object) {}

  JSON_ACCESS_STRING(CloudJobOutput, bash);
  JSON_ACCESS_STRING(CloudJobOutput, report);
};

class CloudSdkInfo : public json::JsonValue {
public:
  CloudSdkInfo() : json::JsonValue(json::JsonObject()) {}
  CloudSdkInfo(const json::JsonObject &object) : json::JsonValue(object) {}
  JSON_ACCESS_STRING(CloudSdkInfo, os);
  JSON_ACCESS_STRING(CloudSdkInfo, path);
  JSON_ACCESS_STRING(CloudSdkInfo, type);
  JSON_ACCESS_STRING(CloudSdkInfo, version);
};

class SdkDocument : public DocumentAccess<SdkDocument> {
public:
  SdkDocument() : DocumentAccess<SdkDocument>("sl", Document::Id("sdk")) {}

  JSON_ACCESS_ARRAY(SdkDocument, CloudSdkInfo, list);
};

class CloudSettingsMbedSupportInfo : public json::JsonValue {
public:
  CloudSettingsMbedSupportInfo() : json::JsonValue(json::JsonObject()) {}
  CloudSettingsMbedSupportInfo(const json::JsonObject &object)
    : json::JsonValue(object) {}

  JSON_ACCESS_STRING(CloudSettingsMbedSupportInfo, name);
  JSON_ACCESS_STRING(CloudSettingsMbedSupportInfo, path);
  JSON_ACCESS_STRING_WITH_KEY(
    CloudSettingsMbedSupportInfo,
    projectId,
    project_id);
};

class CloudSettings : public DocumentAccess<CloudSettings> {
public:
  CloudSettings()
    : DocumentAccess<CloudSettings>("sl", Document::Id("settings")) {}

  CloudSettings(const fs::File &file)
    : DocumentAccess<CloudSettings>("sl", Document::Id("")) {
    to_object() = json::JsonDocument().load(file);
  }

  JSON_ACCESS_STRING_WITH_KEY(
    CloudSettings,
    downloadCMakeMacOsX,
    download_cmake_macosx);
  JSON_ACCESS_STRING_WITH_KEY(
    CloudSettings,
    downloadSdkMacOsX,
    download_sdk_macosx);
  JSON_ACCESS_STRING_WITH_KEY(
    CloudSettings,
    downloadSdkWindows,
    download_sdk_windows);
  JSON_ACCESS_STRING_WITH_KEY(
    CloudSettings,
    downloadCMakeWindows,
    download_cmake_windows);
  JSON_ACCESS_STRING_WITH_KEY(CloudSettings, linuxVersion, linux_version);
  JSON_ACCESS_STRING_WITH_KEY(CloudSettings, macosxVersion, macosx_version);
  JSON_ACCESS_STRING_WITH_KEY(CloudSettings, windowsVersion, windows_version);
  JSON_ACCESS_STRING_WITH_KEY(CloudSettings, installWindows, install_windows);
  JSON_ACCESS_STRING_WITH_KEY(
    CloudSettings,
    downloadDriverWindows,
    download_driver_windows);
  JSON_ACCESS_STRING_WITH_KEY(CloudSettings, sessionTicket, session_ticket);
  JSON_ACCESS_STRING_ARRAY_WITH_KEY(CloudSettings, tags, tag_list);
  JSON_ACCESS_ARRAY_WITH_KEY(
    CloudSettings,
    CloudSettingsMbedSupportInfo,
    mbedSupportList,
    mbed_support_list);
  JSON_ACCESS_BOOL(CloudSettings, global);

private:
};

#endif // CLOUDSETTINGS_HPP
