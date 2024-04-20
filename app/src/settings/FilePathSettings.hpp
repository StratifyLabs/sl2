#ifndef FILEPATHSETTINGS_HPP
#define FILEPATHSETTINGS_HPP

#include <fs/Path.hpp>
#include <sys/System.hpp>
#include <var/String.hpp>

class FilePathSettings {
public:
  static const var::StringView workspace_settings_filename() {
    return "sl_workspace_settings.json";
  }

  static const var::StringView project_settings_filename() {
    return "sl_settings.json";
  }

  static const var::PathString
  project_settings_file_path(const var::StringView project_directory_path) {
    return var::PathString(project_directory_path)
           / project_settings_filename();
  }

  static var::PathString global_directory() {
    return var::PathString(sys::System::user_data_path()) / "sl";
  }

  static const var::PathString workspace_history_filename() {
    return global_directory() / "sl_workspace_history.json";
  }

  static var::PathString global_settings_path() {
    return global_directory() / "sl_global_settings.json";
  }

  static var::PathString global_cloud_settings_path() {
    return global_directory() / "sl_global_cloud_settings.json";
  }

  static var::StringView credentials_path() { return "sl_credentials.json"; }

  static var::PathString global_credentials_path() {
    return global_directory() / credentials_path();
  }
};

#endif // FILEPATHSETTINGS_HPP
