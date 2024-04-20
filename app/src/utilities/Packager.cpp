#include <chrono.hpp>
#include <crypto.hpp>
#include <fs.hpp>
#include <printer.hpp>
#include <sys.hpp>

#include "OperatingSystem.hpp"
#include "Packager.hpp"
#include "settings/PackageSettings.hpp"

Packager::Packager() {
  m_temporary_directory_path
    = PathString(sys::System::user_data_path()) / "sl/packager-"
      & ClockTime().get_system_time().to_unique_string();

  SL_PRINTER_TRACE("creating directory " + m_temporary_directory_path);
  FileSystem().create_directory(
    m_temporary_directory_path,
    Dir::IsRecursive::yes,
    Permissions(0700));
}

Packager::~Packager() {
  // delete the files in the temporary path
  if (!is_keep_temporary()) {
    SL_PRINTER_TRACE("removing directory " + m_temporary_directory_path);
    api::ErrorScope error_scope;
    FileSystem().remove_directory(
      m_temporary_directory_path,
      Dir::IsRecursive::yes);
  } else {
    SL_PRINTER_TRACE("keeping directory " + m_temporary_directory_path);
  }
}

void Packager::do_gather(const PublishOptions &options) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("Packager");
  StringList list;
  const auto filter_list = [&]() {
    if (options.filter().is_empty() == false) {
      return options.filter().split(get_filter_delimeter());
    }
    return StringViewList{};
  }();

  if (options.name() == "sl") {
    SL_PRINTER_TRACE("nothing to gather for sl");
    return;
  }

  get_directory_entries(list, options.source_path());

  SL_PRINTER_TRACE(String().format("filter count is %d", filter_list.count()));
  SL_PRINTER_TRACE(String().format("entry count is %d", list.count()));

  printer().open_array("unfiltered list", printer::Printer::Level::debug);
  for (u32 i = 0; i < list.count(); i++) {
    printer().key(
      String().format("[%d of %d]", i + 1, list.count()),
      list.at(i));
  }
  printer().close_array();

  // apply filters
  for (const auto &filter_entry : filter_list) {
    SL_PRINTER_TRACE("applying filter " + filter_entry);
    for (auto &list_entry : list) {
      if (filter_entry && list_entry.string_view().contains(filter_entry)) {
        SL_PRINTER_TRACE(
          "removed " + list_entry + " using filter " + filter_entry);
        list_entry = "**removed** contained " + filter_entry;
      }
    }
  }

  {
    Printer::Array pa(
      printer().output(),
      "filtered entries",
      Printer::Level::trace);
    printer() << list;
  }

  // copy files from source to destination
  SL_PRINTER_TRACE("copying files to " | m_temporary_directory_path);
  if (options.progress_callback()) {
    options.progress_callback()->update(0, list.count());
  }
  auto count = size_t{};
  for (const auto &item : list) {
    String destination_tail = item;
    if (destination_tail.string_view().find("**removed**") == String::npos) {
      destination_tail(
        String::Erase().set_length(options.source_path().length()));

      const auto destination_path
        = m_temporary_directory_path + destination_tail;
      // make sure the destination directory exists
      StringView parent_directory = Path::parent_directory(destination_path);

      if (!FileSystem().directory_exists(parent_directory)) {
        FileSystem().create_directory(
          parent_directory,
          Dir::IsRecursive::yes,
          Permissions(0755));
      }

      const auto info = FileSystem().get_info(item);
      File(
        File::IsOverwrite::yes,
        destination_path,
        fs::OpenMode::write_only(),
        info.permissions())
        .write(File(item));
    }
    if (options.progress_callback()) {
      options.progress_callback()->update(++count, list.count());
    }
  }
  if (options.progress_callback()) {
    options.progress_callback()->update(0, 0);
  }
}

void Packager::do_archive(const PublishOptions &options) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("Packager");

  const auto info = FileSystem().get_info(options.source_path());

  if (info.is_valid() == false) {
    SL_PRINTER_TRACE("Source path is invalid");
    return;
  }

  if (info.is_file()) {
    m_archive_path = options.source_path();
    SL_PRINTER_TRACE("nothing to archive for sl use:" + m_archive_path);
    return;
  }

  const auto suffix = OperatingSystem::get_executable_suffix().is_empty()
                              ? ".bin"
                              : OperatingSystem::get_executable_suffix();

  const auto archive_name
    = (options.archive_name().is_empty() ? options.name()
                                         : options.archive_name())
      & OperatingSystem::get_archive_suffix();

  const auto archive_file_name = archive_name & suffix;
  m_archive_path = m_temporary_directory_path / archive_file_name;

  OperatingSystem::archive(
    OperatingSystem::ArchiveOptions()
      .set_source_directory_path(m_temporary_directory_path)
      .set_destination_file_path(archive_file_name));

  if (options.is_sblob()) {
    const PathString input_path = m_archive_path;
    m_archive_path = m_temporary_directory_path / archive_name & "."
                     & OperatingSystem::system_name() & "_sblob";


    sos::Auth::create_secure_file(
      sos::Auth::CreateSecureFile()
        .set_key(options.key())
        .set_input_path(input_path)
        .set_output_path(m_archive_path)
        .set_padding_character('\0')
        .set_progress_callback(options.progress_callback())
        .set_remove_key(options.is_remove_key()));

    API_RETURN_IF_ERROR();

  }

  m_hash = Sha256::get_hash(File(m_archive_path));

  if (options.destination().is_empty() == false) {
    File(
      File::IsOverwrite::yes,
      options.destination() & "." & Path::suffix(m_archive_path),
      OpenMode::read_write(),
      Permissions(0755))
      .write(File(m_archive_path));
  }
}

void Packager::do_upload(const PublishOptions &options) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("Packager");

  if (options.is_upload() == false) {
    return;
  }

  PackageSettings package_settings;

  const DateTime timestamp = DateTime::get_system_time();
  const Date date(timestamp);

  var::Vector<PackageDescription> package_description_list
    = options.is_preview() ? package_settings.get_preview_list()
                           : package_settings.get_list();

  package_description_list.push_back(
    PackageDescription()
      .set_name(options.name())
      .set_version(
        options.version().is_empty()
          ? String().format("%d.%d.%d", date.year(), date.month(), date.day())
          : options.version())
      .set_os(sys::System::operating_system_name())
      .set_processor(sys::System::processor())
      .set_type("storage")
      .set_storage_path(Path::name(m_archive_path)));

  if (options.is_preview()) {
    package_settings.set_preview_list(package_description_list);
  } else {
    package_settings.set_list(package_description_list);
  }

  printer().object(
    "package",
    package_description_list.back(),
    Printer::Level::info);

  printer().object(
    "stage package settings",
    package_settings,
    Printer::Level::trace);

  SL_PRINTER_TRACE(
    "creating storage object at " + package_description_list.back().get_path());

  if (options.is_dryrun()) {
    SL_PRINTER_TRACE("abort upload -- just a dry run");
    return;
  }

  cloud_service().storage().create_object(
    package_description_list.back().get_path(),
    File(m_archive_path));

  if (is_success()) {
    SL_PRINTER_TRACE("saving package settings");
    package_settings.save();
  } else {
    SL_PRINTER_TRACE("Error uploading -- didn't save package settings");
  }
}

void Packager::do_download(const DeployOptions &options) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("Packager");

  PackageSettings package_settings;

  API_RETURN_IF_ERROR();

  if (options.is_download() == false) {
    m_archive_path = options.archive_path();
  }

  if (options.url().is_empty()) {
    PackageDescription package_description
      = package_settings.get_package_description(
        PackageSettings::PackageOptions()
          .set_name(options.name())
          .set_version(options.version()));

    if (package_description.is_empty()) {
      SL_PRINTER_TRACE(
        "didn't find " | options.name() | " v" | options.version());
      return;
    }

    m_archive_path = m_temporary_directory_path
                     / fs::Path::name(package_description.get_path());

    cloud_service().storage().get_object(
      package_description.get_path(),
      File(File::IsOverwrite::yes, m_archive_path));
  } else {
    const StringView suffix
      = OperatingSystem::get_executable_suffix().is_empty()
          ? ".bin"
          : OperatingSystem::get_executable_suffix();
    m_archive_path = m_temporary_directory_path / "download" & suffix;

    if (options.url().find("_sblob") != StringView::npos) {
      if (
        options.url().find(OperatingSystem::system_name())
        == StringView::npos) {
        API_RETURN_ASSIGN_ERROR(
          "URL does not match OS system name -> "
            | OperatingSystem::system_name(),
          EINVAL);
      }
      m_archive_path = m_temporary_directory_path / "download."
                       & OperatingSystem::system_name() & "_sblob";
      SL_PRINTER_TRACE("Archive downloading to " + m_archive_path);
    }

    if (
      (options.url().find("http://") == 0)
      || (options.url().find("https://") == 0)) {
      SL_PRINTER_TRACE("downloading");
      sos::Link::File destination_file(File::IsOverwrite::yes, m_archive_path);
      const auto result = OperatingSystem::download_file(
        inet::Url(options.url()),
        destination_file,
        cloud_service().printer().progress_callback());

      if (result.is_empty() == false) {
        API_RETURN_ASSIGN_ERROR(result.cstring(), EINVAL);
      }
    } else {
      SL_PRINTER_TRACE("copying");
      File(File::IsOverwrite::yes, m_archive_path).write(File(options.url()));
    }
  }
}

// extracts directly to its final location
void Packager::do_extract(const DeployOptions &options) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("Packager");
  API_RETURN_IF_ERROR();

  SL_PRINTER_TRACE(
    "Extracting " + m_archive_path + " in "
    + options.destination_directory_path());

  if (options.is_extract() == false) {
    return;
  }

  // check the hash
  {
    m_hash = Sha256::get_hash(File(m_archive_path));
    const auto hash_string = View(m_hash).to_string<GeneralString>();

    if ((options.hash().is_empty() == false) && hash_string != options.hash()) {
      API_RETURN_ASSIGN_ERROR("hash check failed on downloaded image", EINVAL);
      return;
    }
    SL_PRINTER_TRACE("hash is OK");
  }

  if (m_archive_path.string_view().find("_sblob") != StringView::npos) {
    SL_PRINTER_TRACE("decrypting sblob");

    const PathString input_path = m_archive_path;
    m_archive_path
      = input_path & "_exec" & OperatingSystem::get_executable_suffix();

    SL_PRINTER_TRACE("new archive path " | m_archive_path);

    sos::Auth::create_plain_file(
      sos::Auth::CreatePlainFile()
        .set_input_path(input_path)
        .set_output_path(m_archive_path)
        .set_key(options.key())
        .set_progress_callback(options.progress_callback()));
    API_RETURN_IF_ERROR();
  }

  OperatingSystem::extract(
    OperatingSystem::ExtractOptions()
      .set_destination_directory_path(options.destination_directory_path())
      .set_source_file_path(m_archive_path));
}

void Packager::get_directory_entries(
  StringList &list,
  const var::StringView path) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("Packager");
  auto dir_list = FileSystem().read_directory(path);

  for (auto const &entry : dir_list) {
    PathString entry_path = PathString(path) / entry;

    FileInfo info = FileSystem().get_info(entry_path);
    if (info.is_valid() == false) {
      printer().warning("failed to get info for " + entry_path);
    } else {
      if (
        info.is_directory() && entry != "." && entry != ".."
        && entry != "__MACOSX") {
        get_directory_entries(list, entry_path);
      } else if (info.is_file() && entry != ".DS_Store") {
        list.push_back(entry_path.string_view().to_string());
      }
    }
  }
}
