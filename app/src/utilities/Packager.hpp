#ifndef PACKAGER_HPP
#define PACKAGER_HPP

#include "App.hpp"
#include <cloud/Cloud.hpp>
#include <var.hpp>
#include <crypto/Sha256.hpp>

/*
 * sl - single file package
 * compiler - arm-none-eabi binary and related files
 * sdk - headers and library files for targets
 *
 */

class Packager : public AppAccess {
public:
  Packager();
  ~Packager();

  static StringView get_filter_delimeter() { return "?"; }

  class PublishOptions {
    API_AC(PublishOptions, StringView, name); // sl, compiler, sdk
    API_AC(PublishOptions, StringView,
           archive_name); // sl, compiler, sdk
    API_AC(PublishOptions, StringView, source_path);
    API_AC(PublishOptions, StringView, filter);
    API_AC(PublishOptions, StringView, version);
    API_AC(PublishOptions, StringView, destination);
    API_AC(PublishOptions, StringView, key);
    API_AB(PublishOptions, preview, false);
    API_AB(PublishOptions, dryrun, false);
    API_AB(PublishOptions, upload, true);
    API_AB(PublishOptions, sblob, true);
    API_AB(PublishOptions, remove_key, false);
    API_AF(PublishOptions, const api::ProgressCallback*, progress_callback, nullptr);
  };

  Packager& publish(const PublishOptions &options) {
    do_gather(options);
    do_archive(options);
    do_upload(options);
    return *this;
  }

  class DeployOptions {
    API_AC(DeployOptions, StringView, url); // url if not the default
    API_AC(DeployOptions, StringView, name); // sl, compiler, sdk, 7z
    API_AC(DeployOptions, StringView, version);
    API_AC(DeployOptions, StringView, destination_directory_path);
    API_AC(DeployOptions, StringView, key);
    API_AC(DeployOptions, StringView, hash);
    API_AC(DeployOptions, StringView, archive_path);
    API_AB(DeployOptions, extract, true);
    API_AB(DeployOptions, download, true);
    API_AF(DeployOptions, const api::ProgressCallback*, progress_callback, nullptr);

  };

  Packager& deploy(const DeployOptions &options) {
    do_download(options);
    do_extract(options);
    return *this;
  }

  const var::PathString archive_path() const {
    return m_archive_path;
  }

private:
  static constexpr u32 packager_version = 0x00000100;

  API_AB(Packager, keep_temporary, false);
  API_RAC(Packager, var::PathString, temporary_directory_path);
  API_RAC(Packager, crypto::Sha256::Hash, hash);
  PathString m_archive_path;

  void get_directory_entries(StringList &list, const StringView path);
  static StringList get_filter_list(const var::StringView filter_string);

  void do_gather(const PublishOptions &options);
  void do_archive(const PublishOptions &options);
  void do_upload(const PublishOptions &options);

  void do_download(const DeployOptions &options);
  void do_extract(const DeployOptions &options);
};

#endif // PACKAGER_HPP
