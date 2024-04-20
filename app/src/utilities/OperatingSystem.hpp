#ifndef OPERATINGSYSTEM_HPP
#define OPERATINGSYSTEM_HPP

#include <inet/Url.hpp>
#include <sos/Link.hpp>
#include <sys/System.hpp>
#include <var/String.hpp>

class OperatingSystem {
public:
  static var::NameString system_name() {
    return var::NameString(sys::System::operating_system_name())
      .append("_")
      .append(sys::System::processor());
  }
  static var::StringView get_executable_suffix() {
    if (sys::System::is_windows()) {
      return ".exe";
    }
    return "";
  }

  static bool change_mode(const var::StringView file_path, u32 mode);

  static var::StringView get_archive_suffix() { return ".7z"; }

  static var::PathString search_path(const var::StringView application);

  static void resolve_path_to_sl(var::StringView invoked_sl_path);

  static var::PathString get_path_to_sl();

  class ArchiveOptions {
    API_ACCESS_COMPOUND(ArchiveOptions, var::PathString, source_directory_path);
    API_ACCESS_COMPOUND(ArchiveOptions, var::PathString, destination_file_path);
  };

  static bool archive(const ArchiveOptions &options);

  class ExtractOptions {
    API_ACCESS_COMPOUND(ExtractOptions, var::StringView, source_file_path);
    API_ACCESS_COMPOUND(
      ExtractOptions,
      var::StringView,
      destination_directory_path);
  };
  static bool extract(const ExtractOptions &options);

  static var::Vector<sos::Link::DriverPath> get_device_list();

  static var::Vector<sos::Link::DriverPath>
  get_stratify_os_serial_devices_port_list();

  static var::String download_file(const inet::Url url,
    const sos::Link::File & destination_file,
    const api::ProgressCallback* progress_callback);

private:
};

#endif // OPERATINGSYSTEM_HPP
