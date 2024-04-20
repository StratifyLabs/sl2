// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <chrono.hpp>
#include <fs/Dir.hpp>
#include <fs/File.hpp>
#include <printer/Printer.hpp>
#include <var.hpp>

#include "Group.hpp"
#include "WorkspaceSettings.hpp"

using namespace var;

WorkspaceSettings::WorkspaceSettings() {
  load();
  if (is_valid()) {
    if (get_sdk_directory().is_empty()) {
      set_sdk_directory("SDK");
    }
  }
}

int WorkspaceSettings::initialize_workspace() {
  if (is_valid() == false) {
    *this = JsonObject();
    set_sdk_directory("SDK");
    set_creation_timestamp(DateTime::get_system_time().ctime());
  }
  save();
  return 0;
}

void WorkspaceSettings::set_defaults() {}

var::PathString WorkspaceSettings::sl_location() const {
    PathString location = get_sl_location_value();
  if (location.is_empty()) {
    location = Path::parent_directory(OperatingSystem::get_path_to_sl());
  }
  return location;
}

int WorkspaceSettings::generate_configuration_header(const StringView path) {
  Project settings = Project().import_file(
    File(FilePathSettings::project_settings_file_path(path)));
  const PathString configuration_path = PathString(path) / "src/sl_config.h";

  const StringView header
    = "/* Do not modifiy this file.\n * It was generated using the sl command "
      "line tool\n * Change the settings using "
      "the tool then build again using sl\n */\n\n#ifndef "
      "SL_CONFIG_H_\n#define "
      "SL_CONFIG_H_\n\n";

  const sys::Version version(settings.get_version());

  fs::File configuration
    = fs::File(fs::File::IsOverwrite::yes, configuration_path)
        .write(header)
        .write(String().format(
          "#define SL_CONFIG_VERSION_STRING \"%s\"\n",
          version.cstring()))
        .write(String().format(
          "#define SL_CONFIG_VERSION_BCD 0x%02X\n",
          version.to_bcd16()))
        .write(String().format(
          "#define SL_CONFIG_DOCUMENT_ID \"%s\"\n",
          settings.get_document_id_cstring()))
        .write(String().format(
          "#define SL_CONFIG_TEAM_ID \"%s\"\n",
          settings.get_team_id_cstring()))
        .write(String().format(
          "#define SL_CONFIG_NAME \"%s\"\n",
          settings.get_name_cstring()))
        .write(String().format(
          "#define SL_CONFIG_TYPE \"%s\"\n",
          settings.get_type_cstring()))
        .write(String().format(
          "#define SL_CONFIG_PERMISSIONS \"%s\"\n",
          settings.get_permissions_cstring()))
        .write(String().format(
          "#define SL_CONFIG_HARDWARE_ID_STRING \"%s\"\n",
          settings.get_hardware_id_cstring()))
        .write("\n//Thread stack sizes\n")
        .move();

  var::Vector<Project::ThreadStackItem> thread_stack_list
    = settings.get_thread_stack_list();
  for (const auto &item : thread_stack_list) {
    configuration.write(String().format(
      "#define SL_%s_THREAD_STACK_SIZE %dUL\n",
      KeyString(item.key()).to_upper().cstring(),
      item.stack_size()));
  }

  configuration.write(StringView("\n#endif\n\n"));

  return 0;
}

PathString WorkspaceSettings::current_workspace() {
  var::PathString result;
  if (sys::System::is_macosx()) {
    result = getenv("PWD");
  } else {
    result = getenv("CWD");
  }
  return result;
}
