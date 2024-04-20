#ifndef GCOVPARSER_HPP
#define GCOVPARSER_HPP

#include <json.hpp>
#include <var.hpp>

class GcovFileCoverage : public json::JsonKeyValue {
public:
  GcovFileCoverage(const var::StringView key)
    : json::JsonKeyValue(key, json::JsonObject()) {}
  GcovFileCoverage(const var::StringView key, const json::JsonObject &object)
    : json::JsonKeyValue(key, object) {}

  JSON_ACCESS_INTEGER_WITH_KEY(GcovFileCoverage, lineCount, line_count);
  JSON_ACCESS_INTEGER_WITH_KEY(
    GcovFileCoverage,
    lineExecutionCount,
    line_execution_count);
};

class GcovCoverage : public json::JsonObject {
public:
  JSON_ACCESS_OBJECT_LIST_WITH_KEY(
    GcovCoverage,
    GcovFileCoverage,
    sourceList,
    source_list);

  JSON_ACCESS_INTEGER_WITH_KEY(GcovCoverage, lineCount, line_count);
  JSON_ACCESS_INTEGER_WITH_KEY(
    GcovCoverage,
    lineExecutionCount,
    line_execution_count);

private:
  friend class GcovParser;
  GcovCoverage &calculate_line_count() {
    u32 line_count = 0;
    var::Vector<GcovFileCoverage> list = source_list();
    for (const GcovFileCoverage &file_coverage : list) {
      line_count += file_coverage.get_line_count();
    }
    set_line_count(line_count);
    return *this;
  }
  GcovCoverage &calculate_line_execution_count() {
    u32 line_execution_count = 0;
    var::Vector<GcovFileCoverage> list = source_list();
    for (const GcovFileCoverage &file_coverage : list) {
      line_execution_count += file_coverage.get_line_execution_count();
    }
    set_line_execution_count(line_execution_count);
    return *this;
  }
};

#include "../App.hpp"

class GcovParser : public AppAccess {
public:
  GcovParser();

  GcovCoverage parse(const var::StringList &path_list);

private:
  GcovFileCoverage parse_file(const StringView path);
};

#endif // GCOVPARSER_HPP
