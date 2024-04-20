#ifndef PACKAGESETTINGS_HPP
#define PACKAGESETTINGS_HPP

#include <service.hpp>
#include <sys.hpp>
#include <var.hpp>

class PackageDescription : public json::JsonObject {
public:
  PackageDescription() {}
  PackageDescription(const json::JsonObject &object)
    : json::JsonObject(object) {}

  JSON_ACCESS_STRING(PackageDescription, name);
  JSON_ACCESS_STRING(PackageDescription, type);
  JSON_ACCESS_STRING(PackageDescription, path);
  JSON_ACCESS_STRING(PackageDescription, os);
  JSON_ACCESS_STRING(PackageDescription, processor);
  JSON_ACCESS_STRING(PackageDescription, version);

  PackageDescription &set_storage_path(const var::StringView file_name) {
    set_path(
      "packages/" + get_name() + "/v" + get_version() + "/" + get_os() + "_"
      + get_processor() + "/" + file_name);
    return *this;
  }

  sys::Version get_version_string() const {
    return sys::Version(get_version());
  }
};

class PackageSettings : public DocumentAccess<PackageSettings> {
public:
  PackageSettings() : DocumentAccess("sl", Id("packages")) {}

  JSON_ACCESS_ARRAY(PackageSettings, PackageDescription, list);
  JSON_ACCESS_ARRAY_WITH_KEY(
    PackageSettings,
    PackageDescription,
    previewList,
    preview_list);

  void upload() { save(); }

  class PackageOptions {
    API_AC(PackageOptions, var::StringView, name);
    API_AC(PackageOptions, var::StringView, version);
    API_AB(PackageOptions, preview, false);
  };

  PackageDescription
  get_package_description(const PackageOptions &options) const {
    PackageDescription result;
    const Vector<PackageDescription> package_list
      = options.is_preview() ? get_preview_list() : get_list();

    sys::Version latest_version;
    for (const PackageDescription &package : package_list) {
      if (
        package.get_os() == sys::System::operating_system_name()
        && package.get_processor() == sys::System::processor() && package.get_name() == options.name()
        && (options.version().is_empty() ? package.get_version_string() > latest_version : options.version() == package.get_version())) {
        latest_version = package.get_version_string();
        result = package;
      }
    }
    return result;
  }

  static PackageDescription
  get_cloud_package_description(const PackageOptions &options) {
    return PackageSettings().get_package_description(options);
  }
};

#endif // PACKAGESETTINGS_HPP
