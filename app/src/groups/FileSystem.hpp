// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <fs/Path.hpp>

#include "Connector.hpp"

class FileSystemGroup : public Connector {
public:
  FileSystemGroup();

private:
  enum commands {
    command_list,
    command_mkdir,
    command_remove,
    command_copy,
    command_format,
    command_write,
    command_read,
    command_verify,
    command_validate,
    command_exists,
    command_download,
    command_execute,
    command_archive,
    command_bundle,
    command_convert,
    command_total
  };

  chrono::MicroTime m_progress_delay;

  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool list(const Command &command);
  bool mkdir(const Command &command);
  bool remove(const Command &command);
  bool copy(const Command &command);
  bool format(const Command &command);
  bool write(const Command &command);
  bool read(const Command &command);
  bool verify(const Command &command);
  bool validate(const Command &command);
  bool exists(const Command &command);
  bool download(const Command &command);
  bool execute(const Command &command);
  bool archive(const Command &command);
  bool bundle(const Command &command);
  bool convert(const Command &command);


  api::ProgressCallback::IsAbort update_progress_with_delay(int progress, int total) {
    if (m_progress_delay.microseconds() > 0) {
      m_progress_delay.wait();
    }
    printer().update_progress(progress, total);
    return api::ProgressCallback::IsAbort::no;
  }


  bool is_cloud_needed_for_key(const StringView key) {
    return (fs::Path::suffix(key) != "json") && !key.is_empty()
           && (key.length() != 64);
  }

  static void list_assets(const StringView path, const FileObject &assets);
  bool gather_assets(const Command &command);
  static bool decrypt_assets(const Command & command);
  PathString sanitize_asset_destination(const var::StringView value);

  static GeneralString get_cipher_key(const StringView key, const StringView password);

  static bool list_entry(
    const StringView path,
    const StringView entry,
    bool is_details,
    bool is_hide,
    link_transport_mdriver_t *driver);
};

#endif // FILESYSTEM_HPP
