#ifndef GLOBALSETTINGS_HPP
#define GLOBALSETTINGS_HPP

#include <fs.hpp>
#include <json.hpp>

#include "CloudSettings.hpp"
#include "FilePathSettings.hpp"

class GlobalSettings : public JsonObject {
public:
  GlobalSettings() {}
  GlobalSettings(const json::JsonObject &object) : JsonObject(object) {}

  JSON_ACCESS_STRING_WITH_KEY(GlobalSettings, skipUpdate, skip_version_update);
  JSON_ACCESS_STRING_WITH_KEY(GlobalSettings, updateVersion, update_version);

  JSON_ACCESS_INTEGER_WITH_KEY(GlobalSettings, creationDate, creation_date);

  const GlobalSettings &save() const {
    json::JsonDocument().save(
      to_object(),
      fs::File(
        File::IsOverwrite::yes,
        FilePathSettings::global_settings_path()));
    return *this;
  }

  GlobalSettings &load() {
      {
    api::ErrorGuard error_guard;
    to_object() = json::JsonDocument().load(
      fs::File(FilePathSettings::global_settings_path()));
      }
    if (!is_valid()) {
      // doesn't exist
      *this = json::JsonObject();
      set_creation_date(chrono::DateTime::get_system_time().ctime());
      // make sure the directory exists
      FileSystem().create_directory(
        FilePathSettings::global_directory(),
        FileSystem::IsRecursive::yes);
    }
    return *this;
  }

private:
};

#endif // GLOBALSETTINGS_HPP
