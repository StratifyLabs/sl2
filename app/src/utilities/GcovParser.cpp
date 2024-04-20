#include <fs.hpp>

#include "GcovParser.hpp"

GcovParser::GcovParser() {}

GcovCoverage GcovParser::parse(const var::StringList &path_list) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("GcovParser");

  JsonKeyValueList<GcovFileCoverage> coverage_list;

  coverage_list.reserve(path_list.count());
  for (const String &path : path_list) {
    coverage_list.push_back(parse_file(path));
  }

  return GcovCoverage()
    .set_source_list(coverage_list)
    .calculate_line_count()
    .calculate_line_execution_count();
}

GcovFileCoverage GcovParser::parse_file(const var::StringView path) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("GcovParser");

  DataFile data_file = std::move(DataFile().write(File(path)));

  String line;
  u32 line_count = 0;
  u32 line_execution_count = 0;

  while ((line = data_file.get_line()).is_empty() == false) {
    size_t first_colon_position = line.string_view().find(":");

    if (first_colon_position != String::npos) {
      StringView line_intro
        = line.string_view().get_substring_with_length(first_colon_position);

      if (line_intro.find("-") != String::npos) {
        // this line is not considered
      } else {

        line_count++;
        if (line_intro.find("#####") == String::npos) {
          // this line was executed
          line_execution_count++;
        }
      }
    }
  }

  return GcovFileCoverage(path)
    .set_line_count(line_count)
    .set_line_execution_count(line_execution_count);
}
