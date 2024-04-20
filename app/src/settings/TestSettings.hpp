#ifndef SETTINGS_TESTSETTINGS_HPP
#define SETTINGS_TESTSETTINGS_HPP

#include <json.hpp>
#include <var.hpp>

class TestDetails : public json::JsonKeyValue {
public:
  TestDetails(const StringView name, const json::JsonObject &object)
    : json::JsonKeyValue(name, object) {}

  explicit TestDetails(const var::StringView key)
    : JsonKeyValue(key, json::JsonObject()) {}

  JSON_ACCESS_STRING(TestDetails, name);
  JSON_ACCESS_BOOL(TestDetails, enabled);
  JSON_ACCESS_STRING(TestDetails, arguments);
  JSON_ACCESS_STRING_ARRAY_WITH_KEY(TestDetails, sourceList, source_list);
};

class TestSettings : public json::JsonValue {
public:
  TestSettings() : json::JsonValue(json::JsonObject()) {}
  TestSettings(const json::JsonObject &object) : json::JsonValue(object) {}
  JSON_ACCESS_STRING_WITH_KEY(
    TestSettings,
    testDirectory,
    test_directory); // usually test
  JSON_ACCESS_OBJECT_LIST_WITH_KEY(
    TestSettings,
    TestDetails,
    testList,
    test_list);

  PathString get_project_directory_path(
    const var::StringView path,
    const TestDetails &details) const {
    return PathString(path) / get_test_directory() / "units"
           / details.key().string_view();
  }

  PathString get_coverage_directory_path(const var::StringView path) const {
    return PathString(path) / get_test_directory() / "coverage";
  }

  PathString get_coverage_directory_path(
    const var::StringView path,
    const TestDetails &details) const {
    return get_coverage_directory_path(path) / details.key().string_view();
  }

  var::PathString get_report_directory_path(const var::StringView path) const {
    return PathString(path) / get_test_directory() / "reports";
  }

  var::PathString get_report_directory_path(
    const var::String &path,
    const TestDetails &details) const {
    return get_report_directory_path(path) / details.key().string_view();
  }

  var::PathString get_cmake_directory_path(
    const var::String &path,
    const TestDetails &details) const {
    return get_project_directory_path(path, details) / "cmake_arm";
  }
};

#endif // SETTINGS_TESTSETTINGS_HPP
