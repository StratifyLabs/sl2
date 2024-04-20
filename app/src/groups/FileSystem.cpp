// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <chrono.hpp>
#include <fs/File.hpp>
#include <printer.hpp>
#include <sos.hpp>
#include <sos/fs/drive_assetfs.h>
#include <sys.hpp>
#include <var.hpp>

#include "FileSystem.hpp"
#include "utilities/Packager.hpp"

FileSystemGroup::FileSystemGroup() : Connector("filesystem", "fs") {}

var::StringViewList FileSystemGroup::get_command_list() const {

  StringViewList list = {
    "list",
    "mkdir",
    "remove",
    "copy",
    "format",
    "write",
    "read",
    "verify",
    "validate",
    "exists",
    "download",
    "execute",
    "archive",
    "bundle",
    "convert"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool ::FileSystemGroup::execute_command_at(
  u32 list_offset,
  const Command &command) {

  switch (list_offset) {
  case command_list:
    return list(command);
  case command_mkdir:
    return mkdir(command);
  case command_remove:
    return remove(command);
  case command_copy:
    return copy(command);
  case command_format:
    return format(command);
  case command_write:
    return write(command);
  case command_read:
    return read(command);
  case command_verify:
    return verify(command);
  case command_validate:
    return validate(command);
  case command_exists:
    return exists(command);
  case command_download:
    return download(command);
  case command_execute:
    return execute(command);
  case command_archive:
    return archive(command);
  case command_bundle:
    return bundle(command);
  case command_convert:
    return convert(command);
  }

  return false;
}

bool FileSystemGroup::list(const Command &command) {


  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      list,
      "lists the contents of a directory on the connected device or the local "
      "host.")
      + GROUP_ARG_OPT(
        details,
        bool,
        true,
        "show details of each directory entry.")
      + GROUP_ARG_OPT(hide, bool, true, "show hidden files.")
      + GROUP_ARG_REQ(path_p, string, <path>, "path to the target folder.")
      + GROUP_ARG_OPT(
        recursive_r,
        bool,
        false,
        "recursively list files in directories."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView path = command.get_argument_value("path");
  StringView recursive = command.get_argument_value("recursive");
  StringView hide = command.get_argument_value("hide");
  StringView details = command.get_argument_value("details");

  command.print_options(printer());

  sos::Link::Path source_path(path, connection()->driver());

  if (source_path.is_device_path()) {
    SL_PRINTER_TRACE("use connection driver");
    if (!is_connection_ready()) {
      return printer().close_fail();
    }
  } else {
    SL_PRINTER_TRACE("use null link driver");
  }

  SlPrinter::Output printer_output_guard(printer());
  Link::FileSystem source_file_system(source_path.driver());
  FileInfo path_info = source_file_system.get_info(source_path.path());

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR(source_path.path_description() + " does not exist");
  }

  if (Path::suffix(source_path.path()) == "assetfs") {

    if (!Link::FileSystem(source_path.driver()).exists(source_path.path())) {
      APP_RETURN_ASSIGN_ERROR(
        source_path.path_description() | " does not exist");
    }

    Link::File asset_file(
      source_path.path(),
      OpenMode::read_only(),
      source_path.driver());

    list_assets(source_path.path_description(), asset_file);
    return is_success();
  }

  if (details == "true") {
    printer().start_table(
      StringViewList({path, "type", "size", "owner", "mode"}));
  } else {
    printer().start_table(StringViewList({path}));
  }

  PathList list;
  {
    SlPrinter::Object progress_object(printer().output(), "progress");
    if (path_info.is_directory()) {

      struct Context {
        int count = 0;
        Printer *printer = nullptr;
      };

      Context context = {0, &printer().output()};

      printer().output().set_progress_key("reading");

      list = source_file_system.read_directory(
        source_path.path(),
        (recursive == "true" ? FileSystem::IsRecursive::yes
                             : FileSystem::IsRecursive::no),
        [](StringView, void *context) -> bool {
          auto *c = reinterpret_cast<Context *>(context);
          c->printer->update_progress(
            c->count++,
            api::ProgressCallback::indeterminate_progress_total());
          return false;
        },
        &context);

      printer().output().update_progress(0, 0);
      printer().output().set_progress_key("progress");

    } else {

      const String directory
        = source_path.prefix() + Path::parent_directory(source_path.path());

      list.push_back(Path::name(source_path.path()));
      source_path = sos::Link::Path(directory, connection()->driver());
    }

    printer().output().set_progress_key("details");

    int count = 0;
    for (const auto &entry : list) {
      count++;
      printer().output().update_progress(count, list.count());
      list_entry(
        source_path.path(),
        entry,
        details == "true",
        hide == "true",
        source_path.driver());
    }
    printer().output().update_progress(0, 0);
    printer().output().set_progress_key("details");
  }

  {
    SlPrinter::Object items(printer().output(), "entries");

    printer().finish_table();
  }

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR(
      "error while reading " + source_path.path_description());
  }

  return is_success();
}

void FileSystemGroup::list_assets(
  const var::StringView path,
  const fs::FileObject &assets) {

  DataFile asset_data;
  asset_data.write(assets).seek(0);

  u32 count = 0;
  asset_data.read(View(count));

  SL_PRINTER_TRACE(GeneralString().format("asset has %d entries", count));

  printer().start_table(
    StringViewList({path, "type", "size", "owner", "mode"}));
  for (u32 i = 0; i < count; i++) {
    drive_assetfs_dirent_t entry;
    asset_data.read(View(entry));

    printer().append_table_row(StringViewList(
      {StringView(entry.name),
       "file",
       NumberString(entry.size),
       NumberString(entry.uid, "%d"),
       NumberString(entry.mode & 0777, "0%o")}));
  }

  printer().finish_table();
}

bool FileSystemGroup::list_entry(
  const var::StringView path,
  const var::StringView entry,
  bool is_details,
  bool is_hide,
  link_transport_mdriver_t *driver) {
  API_RETURN_VALUE_IF_ERROR(false);
  if (!is_hide || (entry.find(".") != 0)) {
    fs::FileInfo info;

    if (is_details) {
      info = Link::FileSystem(driver).get_info(path / entry);

      if (!info.is_valid()) {
        SL_PRINTER_TRACE("failed to get info for " & path / entry);
      }

      var::String type;
      if (info.is_directory()) {
        type = "directory";
      }
      if (info.is_file()) {
        type = "file";
      }
      if (info.is_device()) {
        type = "device";
      }
      if (info.is_block_device()) {
        type = "block device";
      }
      if (info.is_character_device()) {
        type = "character device";
      }
      if (info.is_socket()) {
        type = "socket";
      }
      printer().append_table_row(StringViewList(
        {entry,
         type,
         NumberString(info.size()),
         NumberString(info.owner(), "%d"),
         NumberString(info.permissions().permissions() & 0777, "0%o")}));

    } else {
      printer().append_table_row(StringViewList({entry}));
    }
  }
  return true;
}

bool FileSystemGroup::mkdir(const Command &command) {


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      mkdir,
      "creates a directory on the connected device. This command will only "
      "work if the filesystem supports directory "
      "creation.")
      + GROUP_ARG_OPT(
        mode,
        string,
        0777,
        "permissions in octal representation of the mode (default is 0777).")
      + GROUP_ARG_REQ(path_p, string, <path>, "path to create."));

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto path = command.get_argument_value("path");
  const auto mode = command.get_argument_value("mode");
  const auto mode_value = mode.to_long(String::Base::octal);

  command.print_options(printer());

  sos::Link::Path link_path(path, connection()->driver());

  if (link_path.is_device_path()) {
    if (!is_connection_ready()) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(link_path.path_description());
  Link::FileSystem fs(link_path.driver());

  if (fs.exists(link_path.path())) {
    printer().key("exists", path);
  } else {
    fs.create_directory(
      link_path.path(),
      FileSystem::IsRecursive::no,
      Permissions(mode_value));

    if (is_success()) {
      printer().key("created", path);
    } else {
      APP_RETURN_ASSIGN_ERROR("failed to create directory");
    }
  }
  return is_success();
}

bool FileSystemGroup::remove(const Command &command) {


  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      remove,
      "The `filesystem.remove` command removes a file or directory from the "
      "connected device.")
      + GROUP_ARG_OPT(
        all,
        bool,
        false,
        "removes all the files in the specified directory (but not the "
        "directory).")
      + GROUP_ARG_REQ(path_p, string, <path>, "path to the file to remove.")
      + GROUP_ARG_OPT(
        recursive_r,
        bool,
        false,
        "removes the directory recursively."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView path = command.get_argument_value("path");
  StringView is_recursive = command.get_argument_value("recursive");
  StringView is_all = command.get_argument_value("all");

  command.print_options(printer());

  sos::Link::Path link_path(path, connection()->driver());

  if (link_path.is_device_path()) {
    if (!is_connection_ready()) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(link_path.path_description());

  Link::FileSystem file_system(link_path.driver());

  FileInfo path_info = file_system.get_info(link_path.path());

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR(link_path.path_description() + " does not exist");
  }

  if (is_all == "true") {
    SL_PRINTER_TRACE("reading directory list");

    PathList list = file_system.read_directory(link_path.path());

    SL_PRINTER_TRACE(String().format("processing %d items", list.count()));

    for (const PathString &entry : list) {

      Link::Path link_file_path(path + "/" + entry, connection()->driver());

      FileInfo info = file_system.get_info(link_file_path.path());

      if (info.is_directory() && (is_recursive == "true")) {

        printer().key(path, String("removing directory"));

        file_system.remove_directory(
          link_file_path.path(),
          Dir::IsRecursive::yes);

        if (is_error()) {
          printer().warning("failed to remove " + path);
        }
      }

      if (info.is_file()) {
        printer().key(path, "removing file");

        file_system.remove(link_file_path.path());
        if (is_error()) {
          printer().warning("failed to remove " + path);
        }
      }
    }

    return is_success();

  } else {

    SL_PRINTER_TRACE("not removing all");

    if (is_recursive == "true") {
      SL_PRINTER_TRACE("start recursive remove");
      FileInfo info = file_system.get_info(link_path.path());

      // recursive is only applicable to directories
      if (info.is_directory()) {

        file_system.remove_directory(link_path.path(), Dir::IsRecursive::yes);

        if (is_error()) {
          return printer().close_fail();
        }
        return is_success();
      }
    }

    file_system.remove(link_path.path());
    if (is_error()) {

      if (path_info.is_directory()) {
        printer().troubleshoot(String().format(
          "You can remove all the contents of an existing directory by using "
          "`sl fs.remove:path=device@%s,all` or to "
          "remove the directory and its contents you `sl "
          "fs.remove:path=%s,recursive`.",
          link_path.path_description().cstring(),
          link_path.path_description().cstring()));
      } else {
        printer().troubleshoot("use `sl fs.remove:path=device@<path>`");
      }
      APP_RETURN_ASSIGN_ERROR(String().format(
        "failed to remove %s",
        link_path.path_description().cstring()));
    } else {
      printer().key("removed", link_path.path_description());
    }
  }
  return is_success();
}

bool FileSystemGroup::copy(const Command &command) {

  // copy a file from host to device or vice-versa


  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      copy,
      "copies files to/from the connected device to/from the host computer. "
      "Paths prefixed with *host@* are on the "
      "host computer while paths prefixed with *device@* are on the connected "
      "device.")
      + GROUP_ARG_REQ(
        destination_dest,
        string,
        <destination>,
        "destination path. If the source is a directory, the destination "
        "should be a directory as well.")
      + GROUP_ARG_OPT(
        hidden,
        bool,
        false,
        "copy hidden files (files that start with '.').")
      + GROUP_ARG_OPT(
        overwrite_o,
        bool,
        true,
        "destination will be overwritten withouth warning.")
      + GROUP_ARG_OPT(
        timestamp_ts,
        bool,
        false,
        "add a unique timestamp string to the destination path.")
      + GROUP_ARG_OPT(recursive_r, bool, false, "copy recursively.")
      + GROUP_ARG_OPT(
        remove,
        bool,
        false,
        "remove the source (only if it is on a device) after it is copied.")
      + GROUP_ARG_REQ(
        source_path,
        string,
        <source>,
        "path to the source file to copy. If the source is a directory, all "
        "files in the directory are copied."));

  if (!command.is_valid(reference, printer())) {
    printer().close_fail();
    return false;
  }

  const StringView source = command.get_argument_value("source");
  const StringView destination = command.get_argument_value("destination");
  const StringView timestamp = command.get_argument_value("timestamp");
  StringView overwrite = command.get_argument_value("overwrite");
  StringView is_recursive = command.get_argument_value("recursive");

  command.print_options(printer());

  const auto get_destination_path = [&]() {
    return timestamp == "true"
             ? Path::no_suffix(destination)
                 & ClockTime()
                     .get_system_time()
                     .to_unique_string()
                     .replace(ClockTime::UniqueString::Replace()
                                .set_old_character('.')
                                .set_new_character('_'))
                     .string_view()
                 & "." & Path::suffix(destination)
             : PathString(destination);
  };

  const Link::Path source_link_path(source, connection()->driver());
  Link::Path destination_link_path(
    get_destination_path(),
    connection()->driver());

  if (
    source_link_path.is_device_path()
    || destination_link_path.is_device_path()) {
    SL_PRINTER_TRACE(
      "check for connection (at least one file is on the device)");
    if (!is_connection_ready()) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());

  Link::FileSystem source_file_system(source_link_path.driver());
  Link::FileSystem destination_file_system(destination_link_path.driver());

  const FileInfo source_info
    = source_file_system.get_info(source_link_path.path());
  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR(
      "failed to get info for " + source_link_path.path_description());
  }

  const FileInfo destination_info
    = destination_file_system.get_info(destination_link_path.path());

  bool is_check_destination_overwrite = false;
  if (is_error()) {
    // destination does not exist
    API_RESET_ERROR();
  } else {
    if (destination_info.is_file()) {
      is_check_destination_overwrite = true;
    }
  }

  PathList source_list;
  if (source_info.is_directory()) {
    SL_PRINTER_TRACE("source is a directory");

    if (destination_info.is_valid() && destination_info.is_file()) {
      APP_RETURN_ASSIGN_ERROR(
        "cannot copy a source directory to an existing file");
    }

    if (!destination_info.is_valid()) {
      SL_PRINTER_TRACE(
        "create destination folder "
        + destination_link_path.path_description());

      destination_file_system.create_directory(
        destination_link_path.path(),
        Dir::IsRecursive::yes,
        Permissions(0777));

      if (is_error()) {
        APP_RETURN_ASSIGN_ERROR("failed to create destination directory");
      }
    } else {
      // destination is a valid directory
      SL_PRINTER_TRACE("destination is a valid directory");
      if (overwrite != "true") {
        SL_PRINTER_TRACE("check destination overwrite");
        is_check_destination_overwrite = true;
      } else {
        SL_PRINTER_TRACE("skip check destination overwrite");
      }
    }

    SL_PRINTER_TRACE("Read list for " + source_link_path.path_description());

    source_list = source_file_system.read_directory(
      source_link_path.path(),
      (is_recursive == "true" ? Dir::IsRecursive::yes : Dir::IsRecursive::no));

    for (auto &entry : source_list) {
      SL_PRINTER_TRACE("preparing to copy " + entry);
    }

  } else {
    source_list.push_back(source_link_path.path());
    if (destination_info.is_valid() && destination_info.is_directory()) {
      destination_link_path = Link::Path(
        destination_link_path.path_description() + "/"
          + Path::name(source_link_path.path()),
        connection()->driver());
    } else if (destination_info.is_file()) {
      if (overwrite != "true") {
        printer().troubleshoot(String().format(
          "Use `sl fs.copy:source=%s,dest=%s,overwrite` to overwrite the "
          "existing file",
          source_link_path.path_description().cstring(),
          destination_link_path.path_description().cstring()));
        APP_RETURN_ASSIGN_ERROR(
          "cannot overwrite the existing file "
          + destination_link_path.path_description());
      }
    }
  }

  for (size_t i = 0; i < source_list.count(); i++) {

    const PathString source_file_path
      = source_info.is_directory() ? source_link_path.path() / source_list.at(i)
                                   : source_list.at(i);
    printer().open_object(source_file_path);
    printer().key(
      "count",
      String().format("%d of %d", i + 1, source_list.count()));

    String progress_message;
    Link::Path destination_path;
    Link::Path source_path;

    progress_message = "copying";
    if (source_info.is_directory()) {
      destination_path = Link::Path(
        destination_link_path.path_description() / source_list.at(i),
        connection()->driver());
      source_path = Link::Path(
        source_link_path.prefix() & source_file_path,
        connection()->driver());

    } else {
      destination_path = destination_link_path;
      source_path = Link::Path(
        source_link_path.prefix() & source_file_path,
        connection()->driver());
      SL_PRINTER_TRACE(
        "copying one file " + destination_path.path_description());
    }

    SL_PRINTER_TRACE(String().format(
      "check destination overwrite? %d",
      is_check_destination_overwrite));

    bool is_source_hidden = fs::Path::is_hidden(source_link_path.path());
    bool is_destination_valid = false;
    bool is_destination_appfs = destination_link_path.is_device_path()
                                && (destination_path.path().find("/app") == 0);

    if (is_check_destination_overwrite) {
      is_destination_valid
        = destination_file_system.get_info(destination_link_path.path())
            .is_valid();
    }

    chrono::ClockTimer transfer_timer;
    FileInfo local_source_info
      = source_file_system.get_info(source_link_path.path());

    printer().key("source", source_path.path_description());
    printer().key("destination", destination_path.path_description());

    if (
      (is_check_destination_overwrite && is_destination_valid
       && (overwrite != "true"))
      || is_source_hidden) {
      if ((is_check_destination_overwrite && is_destination_valid)) {
        printer().output_key("skipping", "exists");
      } else if (is_source_hidden) {
        printer().output_key("skipping", "hidden");
      }
    } else {

      printer().output().progress_key() = progress_message;
      SL_PRINTER_TRACE(
        String("do the copy ") + source_path.path_description() + " to "
        + destination_path.path_description());

      int file_copy_result = -1;
      if (is_destination_appfs) {

        if (is_destination_valid) {
          printer().key("removing", destination_path.path_description());
          Link::FileSystem(destination_path.driver())
            .remove(destination_path.path());
        }

        if (!Link::FileSystem(source_path.driver())
               .exists(source_path.path())) {
          APP_RETURN_ASSIGN_ERROR(
            source_path.path_description() | " does not exist");
        }

        Link::File source_file = Link::File(
          source_path.path(),
          OpenMode::read_only(),
          source_path.driver());

        Appfs(
          Appfs::Construct()
            .set_name(destination_path.path())
            .set_size(source_file.size()),
          destination_file_system.driver())
          .append(source_file, printer().output().progress_callback());

        if (is_error()) {

          return printer().close_fail();
        }

      } else {

        SL_PRINTER_TRACE(
          String("about to copy ") + source_path.path_description() + " to "
          + destination_path.path_description());

        // make sure the destintion parent directory exists
        const PathString destination_parent_path
          = Path::parent_directory(destination_path.path());

        if (
          !destination_parent_path.is_empty()
          && !destination_file_system.directory_exists(
            destination_parent_path)) {

          if (is_recursive == "true") {

            destination_file_system.create_directory(
              destination_parent_path,
              Dir::IsRecursive::yes);

            if (is_error()) {
              APP_RETURN_ASSIGN_ERROR(
                "failed to create destination parent folder "
                + destination_path.path_description());
            }
          } else {
            printer().troubleshoot(
              "Use `recursive=true` to automatically create destination "
              "directories.");
            APP_RETURN_ASSIGN_ERROR(
              "destination directory " + destination_path.path_description()
              + " does not exist");
          }
        }

        transfer_timer.start();

        Link::File(
          File::IsOverwrite::yes,
          destination_path.path(),
          OpenMode::append_read_write(),
          Permissions(0777),
          destination_path.driver())
          .write(
            Link::File(
              source_path.path(),
              OpenMode::read_only(),
              source_path.driver()),
            File::Write().set_progress_callback(
              printer().output().progress_callback()));

        transfer_timer.stop();
      }
      printer().output().progress_key() = "progress";

      if (is_error()) {
        printer().debug(
          GeneralString().format("File copy result is %d", file_copy_result));

        if (!local_source_info.is_valid()) {
          printer().troubleshoot(
            "The file `" | source_path.path_description()
            | "` could not be found.");
        }

        if (destination_info.is_directory()) {

          if (!destination_file_system.directory_exists(
                destination_path.path())) {
            printer().troubleshoot(
              String("The destination directory `") + destination_path.path()
              + "` could not be found or created.");
          }
        }

        APP_RETURN_ASSIGN_ERROR("failed to copy " + source_file_path);
      } else {
        printer().transfer_info(
          source_path.path(),
          transfer_timer,
          source_path.driver());
      }
    }
    printer().close_object();
  }
  return is_success();
}

bool FileSystemGroup::format(const Command &command) {

  // copy a file from host to device or vice-versa


  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(format, "formats the filesystem on the device. ")
      + GROUP_ARG_REQ(
        path_p,
        string,
        <path>,
        "path to the filesystem to format."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView path = command.get_argument_value("path");

  const Link::Path link_path(path, connection()->driver());

  command.print_options(printer());

  if (link_path.is_device_path() && !is_connection_ready()) {
    return printer().close_fail();
  }
  SlPrinter::Output printer_output_guard(printer());

  if (link_path.is_host_path()) {
    APP_RETURN_ASSIGN_ERROR("cannot format a `host@` path");
  }

  printer().key(link_path.path_description(), "format");
  connection()->format(link_path.path());

  printer().troubleshoot(
    "**Note.** The format command initiates the filesystem format but does "
    "not wait for it to finish successfully. "
    "Please check the filesystem after it has had time to format. During "
    "this time the device may seem "
    "unresponsive.");

  return is_success();
}

bool FileSystemGroup::write(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      write,
      "writes to a file (or device) on the device. The file will be opened, "
      "written, then closed.")
      + GROUP_ARG_OPT(
        append_a,
        bool,
        false,
        "destination file is opened for appending.")
      + GROUP_ARG_OPT(
        blank,
        int,
        <none>,
        "value of a blank byte. If the entire page is blank on the source, it "
        "will be skipped. This is useful when "
        "programming flash memory.")
      + GROUP_ARG_OPT(
        delay,
        int,
        0,
        "number of milliseconds to delay between chunk writes.")
      + GROUP_ARG_REQ(
        destination_dest,
        string,
        <destination>,
        "destination path of the device file (or device) which will be "
        "written.")
      + GROUP_ARG_OPT(
        location_loc,
        int,
        0,
        "location in the destination file to start writing.")
      + GROUP_ARG_OPT(
        pagesize,
        int,
        512,
        "chunk size for reading the source and writing the destination.")
      + GROUP_ARG_OPT(
        readwrite_rw,
        bool,
        false,
        "destination file is opened for in read/write mode (default is write "
        "only).")
      + GROUP_ARG_OPT(
        size_s,
        int,
        <all>,
        "maximum number of bytes to write to the destination file (default is "
        "write all available bytes).")
      + GROUP_ARG_OPT(
        source_path,
        string,
        <source>,
        "path to the host file which will be written o the device's file (or "
        "device).")
      + GROUP_ARG_OPT(
        text_string,
        string,
        <none>,
        "If source is not provided, use this to write a string directly to the "
        "destination."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView source = command.get_argument_value("source");
  StringView destination = command.get_argument_value("destination");
  StringView text = command.get_argument_value("text");
  StringView is_append = command.get_argument_value("append");
  StringView page_size = command.get_argument_value("pagesize");
  StringView is_readwrite = command.get_argument_value("readwrite");
  StringView delay = command.get_argument_value("delay");
  StringView location = command.get_argument_value("location");
  StringView size = command.get_argument_value("size");
  StringView blank = command.get_argument_value("blank");

  Link::Path source_link_path(source, connection()->driver());
  Link::Path destination_link_path(destination, connection()->driver());

  command.print_options(printer());

  if (
    (!source.is_empty() && source_link_path.is_device_path())
    || destination_link_path.is_device_path()) {
    if (!is_connection_ready()) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(
    source.is_empty() ? text
                      : source_link_path.path_description().string_view());

  if (page_size.to_integer() <= 0) {
    APP_RETURN_ASSIGN_ERROR("invalid page size specified");
  }

  FileObject *s;
  DataFile s_text(OpenMode::read_only());
  Link::File s_file;

  if (source.is_empty()) {
    if (text.is_empty()) {
      APP_RETURN_ASSIGN_ERROR("either `source` or `text` must be provided");
    }

    s_text.data().copy(text);
    SL_PRINTER_TRACE(
      "writing `" + s_text.data().view().to_string<String>() + "` to dest");
    s_text.seek(0);
    s = &s_text;
  } else {
    s = &s_file;
    SL_PRINTER_TRACE("Open source " + source_link_path.path_description());
    s_file = Link::File(
               source_link_path.path(),
               OpenMode::read_only(),
               source_link_path.driver())
               .move();
  }

  ClockTimer transfer_timer;
  transfer_timer.start();

  OpenMode o_flags = OpenMode::write_only();
  if (is_readwrite == "true") {
    o_flags.set_read_write();
  }
  if (is_append == "true") {
    o_flags.set_append();
  }

  SL_PRINTER_TRACE(
    "Open destination " + destination_link_path.path_description());
  auto d = Link::File(
                   destination_link_path.path(),
                   OpenMode::read_write(),
                   destination_link_path.driver())
                   .seek(location.to_integer())
                   .move();

  auto progress_with_delay
    = api::ProgressCallback([this](int value, int total) {
        return update_progress_with_delay(value, total);
      });
  m_progress_delay = delay.to_integer() * 1_milliseconds;

  u32 size_to_write = size.to_unsigned_long();
  if (size_to_write == 0) {
    size_to_write = s->size();
  }

  const size_t page_size_value
    = page_size.to_unsigned_long() ? page_size.to_unsigned_long() : 512;

  printer().output().set_progress_key("writing");
  if (!blank.is_empty()) {

    u32 bytes_written = 0;
    Data buffer(page_size_value);
    Data blank_buffer(page_size_value);

    View(blank_buffer).fill<u8>(blank.to_unsigned_long());

    if ((buffer.size() == 0) || (blank_buffer.size() == 0)) {
      APP_RETURN_ASSIGN_ERROR("failed to allocate memory for page buffers");
    }
    do {
      View(buffer).fill<u8>(blank.to_unsigned_long());
      s->read(buffer);

      if (return_value() > 0) {

        if (size_to_write - bytes_written < buffer.size()) {
          buffer.resize(size_to_write - bytes_written);
        }

        blank_buffer.resize(buffer.size());
        if (blank_buffer != buffer) {
          d.write(buffer);
          if (m_progress_delay.milliseconds() > 0) {
            m_progress_delay.wait();
          }
        } else {
          d.seek(buffer.size(), File::Whence::current);
        }
        bytes_written += buffer.size();
        printer().update_progress(
          static_cast<int>(bytes_written),
          static_cast<int>(size_to_write));
      } else {
        printer().update_progress(0, 0);
      }

    } while ((buffer.size() == page_size_value) && (d.return_value() > 0)
             && (s->return_value() > 0));

    printer().update_progress(0, 0);

  } else {
    SL_PRINTER_TRACE("Writing source to dest " + NumberString(s->size()));
    d.write(
      *s,
      File::Write()
        .set_page_size(page_size_value)
        .set_size(size_to_write)
        .set_progress_callback(&progress_with_delay));
  }
  printer().output().set_progress_key("progress");

  transfer_timer.stop();

  {
    Printer::Object po(printer().active_printer(), "transfer");
    printer().key("size", String().format("%d", size_to_write));
    printer().key(
      "duration",
      String().format(
        "%0.3fs",
        transfer_timer.milliseconds() * 1.0f / 1000.0f));
    printer().key(
      "rate",
      String().format(
        "%0.3fKB/s",
        size_to_write * 1.0f / transfer_timer.milliseconds() * 1000.0f
          / 1024.0f));
  }

  return is_success();
}

bool FileSystemGroup::read(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      read,
      "reads from a file (or device). The file will be opened, read, then "
      "closed. The most common way this command is "
      "used is for reading for the target device's device filesystem (such as "
      "`/dev/drive0`). A block device (on the "
      "target device) can be read and written to a file on the host.")
      + GROUP_ARG_OPT(
        append_a,
        bool,
        false,
        "destination file is opened for appending.")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <binary blob>,
        "destination path to the device or file where the data will be "
        "written.")
      + GROUP_ARG_OPT(
        blob_binary,
        bool,
        <auto>,
        "if a destination is not provided, and `blob` is true, the data will "
        "print as a binary blob. If `blob` is not "
        "specified, the output will either be a blob or text depending on the "
        "content.")
      + GROUP_ARG_OPT(
        location_loc,
        int,
        0,
        "location in the source file to start reading.")
      + GROUP_ARG_OPT(
        pagesize_chunk,
        int,
        512,
        "chunk size for reading the source and writing the destination.")
      + GROUP_ARG_OPT(
        readwrite_rw,
        bool,
        false,
        "destination file is opened for in read/write mode (default is read "
        "only).")
      + GROUP_ARG_OPT(
        size_s,
        int,
        <all>,
        "maximum number of bytes to read from the source file (default is read "
        "all).")
      + GROUP_ARG_REQ(
        source_path,
        string,
        <source>,
        "path to the device or file which will be read."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView source = command.get_argument_value("source");
  const StringView destination = command.get_argument_value("destination");
  StringView is_append = command.get_argument_value("append");
  StringView blob = command.get_argument_value("blob");
  StringView chunk_size = command.get_argument_value("pagesize");
  StringView location = command.get_argument_value("location");
  StringView size = command.get_argument_value("size");

  Link::Path source_link_path(source, connection()->driver());
  Link::Path destination_link_path(destination, connection()->driver());

  command.print_options(printer());

  if (
    source_link_path.is_device_path()
    || (!destination.is_empty() && destination_link_path.is_device_path())) {
    if (!is_connection_ready()) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(source);
  if (chunk_size.to_integer() <= 0) {
    return printer().close_fail();
  }

  // is a read to <assets>.assetfs/<filename>?
  const auto is_assetfs = [&]() {
    if (source_link_path.is_device_path()) {
      return false;
    }
    const auto parent_directory
      = Path::parent_directory(source_link_path.path());
    const auto suffix = Path::suffix(parent_directory);
    return suffix == "assetfs";
  }();

  DataFile asset_file;
  Link::File link_source_file;

  if (is_assetfs) {

    const auto file_path = Path::parent_directory(source_link_path.path());

    if (!FileSystem().exists(file_path)) {
      APP_RETURN_ASSIGN_ERROR(source | " assetfs does not exist");
    }

    Data asset_data = DataFile().write(File(file_path)).data();
    DataFile assetsfs_file(OpenMode::read_only());
    assetsfs_file.data() = asset_data;

    u32 count = 0;
    assetsfs_file.read(View(count));
    const auto file_name = Path::name(source_link_path.path());
    for (const auto i : api::Index(count)) {
      MCU_UNUSED_ARGUMENT(i);
      drive_assetfs_dirent_t entry;
      assetsfs_file.read(View(entry));
      if (file_name == entry.name) {
        asset_file
          .write(
            assetsfs_file.seek(int(entry.start)),
            File::Write().set_size(size_t(entry.size)))
          .seek(0);
        break;
      }
    }

    if (asset_file.size() == 0) {
      APP_RETURN_ASSIGN_ERROR(
        "failed to find " | file_name | " within assetfs " | source);
    }

  } else {

    if (!Link::FileSystem(source_link_path.driver())
           .exists(source_link_path.path())) {
      APP_RETURN_ASSIGN_ERROR(
        source_link_path.path_description() | " does not exist");
    }

    link_source_file = Link::File(
                         source_link_path.path(),
                         OpenMode::read_only(),
                         source_link_path.driver())
                         .seek(location.to_integer())
                         .move();
  }

  FileObject *source_file = &link_source_file;
  if (is_assetfs) {
    source_file = &asset_file;
  }

  SL_PRINTER_TRACE(
    String().format("reading %d bytes from source", source_file->size()));

  const u32 size_to_read = size.to_integer() > 0      ? size.to_integer()
                           : source_file->size() == 0 ? static_cast<u32>(-1)
                                                      : source_file->size();

  const u32 chunk_size_value
    = chunk_size.to_unsigned_long() ? chunk_size.to_unsigned_long() : 512;

  ClockTimer verify_timer;

  if (destination.is_empty()) {
    DataFile temp_destination(OpenMode::append_write_only());

    printer().output().set_progress_key("reading");
    verify_timer.start();

    temp_destination.write(
      *source_file,
      File::Write()
        .set_page_size(chunk_size_value)
        .set_size(size_to_read)
        .set_progress_callback(printer().progress_callback()));

    verify_timer.stop();
    printer().output().set_progress_key("progress");

    bool is_text = true;
    for (u32 i = 0; i < temp_destination.data().size(); i++) {
      u8 value = View(temp_destination.data()).at_const_u8(i);
      if ((value >= 128) || (value < 8)) {
        is_text = false;
      }
    }

    if (temp_destination.data().size() > 0) {

      if ((blob == "true") || !is_text) {
        printer().active_printer().enable_flags(Printer::Flags::blob);
        printer().object("data", temp_destination.data());
      } else {
        printer().open_external_output();
        if (printer().is_json()) {
          printer().key(
            "base64",
            Base64().encode(temp_destination.data().add_null_terminator()));
        } else {
          printer() << StringView(
            temp_destination.data().add_null_terminator());
        }
        printer().close_external_output();
      }
    } else {
      APP_RETURN_ASSIGN_ERROR("no data is available to be read");
    }

  } else {

    // destination parent must exist
    const auto destination_parent
      = Path::parent_directory(destination_link_path.path());

    if (
      !destination_parent.is_empty()
      && !Link::FileSystem(destination_link_path.driver())
            .directory_exists(destination_parent)) {
      APP_RETURN_ASSIGN_ERROR(destination_parent | " directory not found");
    }

    const Link::File destination_file = (is_append == "true")
                                          ? Link::File(
                                              destination_link_path.path(),
                                              OpenMode::append_write_only(),
                                              destination_link_path.driver())
                                              .move()
                                          : Link::File(
                                              fs::File::IsOverwrite::yes,
                                              destination_link_path.path(),
                                              OpenMode::append_write_only(),
                                              Permissions(0777),
                                              destination_link_path.driver())
                                              .move();

    printer().output().set_progress_key("reading");
    verify_timer.start();
    destination_file.write(
      *source_file,
      File::Write()
        .set_page_size(chunk_size_value)
        .set_size(size_to_read)
        .set_progress_callback(printer().progress_callback()));

    verify_timer.stop();
  }

  if (!is_assetfs) {
    const Link::Path &transfer_source
      = source_link_path.is_host_path() || destination.is_empty()
          ? source_link_path
          : destination_link_path;
    printer().transfer_info(
      transfer_source.path(),
      verify_timer,
      transfer_source.driver());
  }

  return is_success();
}

bool FileSystemGroup::verify(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      verify,
      "verifies the source and destination files are equivalent.")
      + GROUP_ARG_REQ(
        destination_dest,
        string,
        <destination>,
        "destination path to the device or file where the data will be "
        "written.")
      + GROUP_ARG_OPT(
        location_loc,
        int,
        0,
        "location in the source file to start reading.")
      + GROUP_ARG_OPT(
        pagesize_chunk,
        int,
        512,
        "chunk size for reading the source and writing the destination.")
      + GROUP_ARG_OPT(
        size_s,
        int,
        <all>,
        "maximum number of bytes to read from the source file (default is read "
        "all).")
      + GROUP_ARG_REQ(
        source_path,
        string,
        <source>,
        "path to the device or file which will be read."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView source = command.get_argument_value("source");
  const StringView destination = command.get_argument_value("destination");
  StringView chunk_size = command.get_argument_value("pagesize");
  StringView location = command.get_argument_value("location");
  StringView size = command.get_argument_value("size");

  Link::Path destination_link_path(destination, connection()->driver());
  Link::Path source_link_path(source, connection()->driver());

  command.print_options(printer());

  if (
    (source_link_path.is_device_path()
     || destination_link_path.is_device_path())
    && !is_connection_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(source_link_path.path_description());

  if (chunk_size.to_integer() <= 0) {
    return printer().close_fail();
  }

  // both files must exist
  if (!Link::FileSystem(destination_link_path.driver())
         .exists(destination_link_path.path())) {
    APP_RETURN_ASSIGN_ERROR(
      destination_link_path.path_description() | " does not exist");
  }

  if (!Link::FileSystem(source_link_path.driver())
         .exists(source_link_path.path())) {
    APP_RETURN_ASSIGN_ERROR(
      source_link_path.path_description() | " does not exist");
  }

  ClockTimer verify_timer;
  verify_timer.start();

  Link::File d = Link::File(
                   destination_link_path.path(),
                   OpenMode::read_only(),
                   destination_link_path.driver())
                   .seek(location.to_integer())
                   .move();

  Link::File s = Link::File(
                   source_link_path.path(),
                   OpenMode::read_only(),
                   source_link_path.driver())
                   .move();

  u32 size_to_read = s.size();

  if (size.to_integer() > 0) {
    size_to_read = size.to_unsigned_long();
  } else if (s.size() == 0) {
    size_to_read = static_cast<u32>(-1);
    if (d.size() != 0) {
      size_to_read = d.size();
    }
  }

  u32 chunk_size_value = chunk_size.to_unsigned_long();
  if (chunk_size_value == 0) {
    chunk_size_value = 512;
  }

  DataFile destination_data_file;
  DataFile source_data_file;

  {
    SlPrinter::Object destination_object(printer().output(), "destination");
    printer()
      .output()
      .key("path", destination_link_path.path())
      .set_progress_key("progress");
    destination_data_file.write(
      d,
      File::Write()
        .set_page_size(chunk_size_value)
        .set_size(size_to_read)
        .set_progress_callback(printer().progress_callback()));

    printer().transfer_info(
      destination_link_path.path(),
      verify_timer,
      destination_link_path.driver());
  }

  {
    SlPrinter::Object destination_object(printer().output(), "source");
    printer()
      .output()
      .key("path", source_link_path.path())
      .set_progress_key("progress");
    printer().output().set_progress_key("progress");
    source_data_file.write(
      s,
      File::Write()
        .set_page_size(chunk_size_value)
        .set_size(size_to_read)
        .set_progress_callback(printer().progress_callback()));

    printer().transfer_info(
      source_link_path.path(),
      verify_timer,
      source_link_path.driver());
  }

  const auto is_match = destination_data_file.data() == source_data_file.data();
  printer().key_bool("match", is_match);
  if (!is_match) {
    APP_RETURN_ASSIGN_ERROR("files do not match");
  }

  return is_success();
}

bool FileSystemGroup::exists(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(exists, "checks if the specified file or directory exists.")
      + GROUP_ARG_REQ(
        path_p,
        string,
        <path>,
        "path to check for an existing file or directory."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView path = command.get_argument_value("path");
  Link::Path link_path(path, connection()->driver());
  command.print_options(printer());

  if (link_path.is_device_path()) {
    if (!is_connection_ready()) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());

  printer().open_object(link_path.path_description());
  std::ignore = Link::FileSystem(link_path.driver()).get_info(link_path.path());
  printer().key_bool("exists", is_success());

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("does not exist");
  }

  return is_success();
}

bool FileSystemGroup::validate(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      validate,
      "validates the contents of the file based on the suffix.")
      + GROUP_ARG_REQ(
        path_p,
        string,
        <path>,
        "path to check for an existing file.")
      + GROUP_ARG_OPT(
        key_k,
        string,
        <none>,
        "key to validate (must begin with `/`.")
      + GROUP_ARG_OPT(value_v, string, <none>, "expected value of `key`."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView path = command.get_argument_value("path");
  const StringView key = command.get_argument_value("key");
  const StringView value = command.get_argument_value("value");

  Link::Path link_path(path, connection()->driver());
  command.print_options(printer());

  if (link_path.is_device_path()) {
    if (!is_connection_ready()) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(link_path.path_description());

  if (!Link::FileSystem(link_path.driver()).exists(link_path.path())) {
    APP_RETURN_ASSIGN_ERROR(link_path.path_description() + " does not exist");
  }

  const StringView suffix = fs::Path::suffix(link_path.path());
  if (suffix == "json") {

    if (!Link::FileSystem(link_path.driver()).exists(link_path.path())) {
      APP_RETURN_ASSIGN_ERROR(link_path.path_description() | " does not exist");
    }

    const Link::File input_file(
      link_path.path(),
      OpenMode::read_only(),
      link_path.driver());
    JsonDocument json_document;
    JsonValue json_parent = json_document.load(input_file);

    if (is_error()) {
      printer().object("parse", json_document.error());
      APP_RETURN_ASSIGN_ERROR("parsing failed");
    } else {
      printer().key("parse", "success");
    }

    if (!key.is_empty()) {
      if (key.at(0) != '/') {
        APP_RETURN_ASSIGN_ERROR("key must start with `/` for the root object");
      }

      if (value.is_empty()) {
        APP_RETURN_ASSIGN_ERROR(
          "A `value` must be specified to check for a match with `key`");
      }

      JsonValue json_value = json_parent.lookup(key);
      if (json_value.is_object()) {
        APP_RETURN_ASSIGN_ERROR(
          "`key` specifies an object within the JSON document");
      }

      if (json_value.is_array()) {
        APP_RETURN_ASSIGN_ERROR(
          "`key` specifies an array within the JSON document");
      }

      const auto json_string = json_value.to_string();
      if (json_string != value) {
        APP_RETURN_ASSIGN_ERROR(
          "`" | key | "` is `" | json_string | "` not `" | value | "`");
      }

      printer().key(key, json_string);
    }

  } else {
    APP_RETURN_ASSIGN_ERROR("unsupported suffix");
  }

  return is_success();
}

bool FileSystemGroup::convert(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(convert, "converts files from xml to JSON format.")
      + GROUP_ARG_REQ(
        source_path,
        string,
        <path>,
        "path to the xml input file.")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <none>,
        "path to the JSON output file.")
      + GROUP_ARG_OPT(flat, bool, false, "Use a flat XML structure"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView source = command.get_argument_value("source");
  const StringView destination = command.get_argument_value("destination");
  const StringView is_flat = command.get_argument_value("flat");

  command.print_options(printer());

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(source);

  if (!FileSystem().exists(source)) {
    APP_RETURN_ASSIGN_ERROR(source + " does not exist");
  }

  const auto effective_destination = [&]() {
    if (destination.is_empty()) {
      return PathString(Path::no_suffix(source) & ".json");
    }
    return PathString(destination);
  }();

  if (const auto destination_parent
      = Path::parent_directory(effective_destination);
      !destination_parent.is_empty()
      && !FileSystem().exists(destination_parent)) {
    APP_RETURN_ASSIGN_ERROR("cannot create output file " | destination);
  }

  const auto suffix = fs::Path::suffix(source);
  const auto destination_suffix = fs::Path::suffix(destination);
  if (suffix == "xml") {

    DataFile input_file;
    input_file.write(File(source));
    printer().output().key("destination", effective_destination);

    const auto is_xml_flat = is_flat != "true" ? JsonDocument::IsXmlFlat::no
                                               : JsonDocument::IsXmlFlat::yes;

    const auto json_value = JsonDocument().from_xml_string(
      input_file.data().add_null_terminator(),
      is_xml_flat);

    JsonDocument().save(
      json_value,
      File(File::IsOverwrite::yes, effective_destination));

  } else {
    APP_RETURN_ASSIGN_ERROR("unsupported suffix");
  }

  return is_success();
}

bool FileSystemGroup::download(const Command &command) {
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();


  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(download, "downloads a file from the internet.")
      + GROUP_ARG_REQ(
        destination_dest,
        string,
        <destination>,
        "file path destination (can be on the host or a connected device).")
      + GROUP_ARG_OPT(overwrite, bool, true, "overwrite the file if it exists.")
      + GROUP_ARG_OPT(
        hash,
        string,
        <hone>,
        "Sha256 hash of downloaded file will be checked if provided.")
      + GROUP_ARG_REQ(url, string, <url>, "URL to download."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView url = command.get_argument_value("url");
  StringView dest = command.get_argument_value("destination");
  StringView overwrite = command.get_argument_value("overwrite");
  StringView hash = command.get_argument_value("hash");

  Link::Path link_path(dest, connection()->driver());
  command.print_options(printer());

  if (!is_cloud_ready()) {
    return printer().close_fail();
  }

  if (link_path.driver() != nullptr) {
    if (!is_connection_ready()) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(url);

  const Link::FileSystem fs(link_path.driver());

  if (overwrite != "true") {
    if (fs.exists(link_path.path())) {
      APP_RETURN_ASSIGN_ERROR(
        "download would overwrite destination " + link_path.path_description());
    }
  }

  const auto parent_directory = Path::parent_directory(link_path.path());
  if (!parent_directory.is_empty() && !fs.directory_exists(parent_directory)) {
    APP_RETURN_ASSIGN_ERROR(parent_directory | " directory not found");
  }

  SL_PRINTER_TRACE("creating output file at " + link_path.path_description());
  {
    const Link::File destination_file = Link::File(
                                          File::IsOverwrite::yes,
                                          link_path.path(),
                                          OpenMode::append_write_only(),
                                          Permissions(0777),
                                          link_path.driver())
                                          .move();

    ClockTimer transfer_timer;
    transfer_timer.start();
    SL_PRINTER_TRACE("getting file at " + url);

    printer().output().set_progress_key("downloading");

    inet::Url u(url);
    String result = OperatingSystem::download_file(
      u,
      destination_file,
      printer().progress_callback());
    if (!result.is_empty()) {
      APP_RETURN_ASSIGN_ERROR(result.string_view());
    }
    transfer_timer.stop();
    printer().output().set_progress_key("progress");
    if (is_success()) {
      printer().transfer_info(link_path, transfer_timer);
    }
  }

  const auto check_hash = crypto::Sha256::get_hash(
    Link::File(link_path.path(), OpenMode::read_only(), link_path.driver()));
  printer().key("hash", View(check_hash).to_string<GeneralString>());
  if (!hash.is_empty()) {
    if (View(check_hash).to_string<GeneralString>() != hash) {
      APP_RETURN_ASSIGN_ERROR("hashes do not match");
    }
  }

  return is_success();
}

bool FileSystemGroup::execute(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(execute, "executes a system command.")
      + GROUP_ARG_REQ(command_cmd, string, <command>, "command to execute.")
      + GROUP_ARG_OPT(
        directory_dir,
        string,
        <current>,
        "the working directory (requires cmake)."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto cmd = command.get_argument_value("command");
  const auto directory = command.get_argument_value("directory");

  command.print_options(printer());
  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(cmd);

  const auto arguments = [&]() {
    const auto arguments = Tokenizer(
      cmd,
      Tokenizer::Construct().set_delimiters(" ").set_ignore_between("\""));
    Process::Arguments result(Process::which(arguments.list().at(0)));

    for (const auto index : api::Range<size_t>(1, arguments.list().count())) {
      const auto arg = arguments.list().at(index);
      result.push(arg);
    }
    return result;
  }();

  execute_system_command(arguments, directory.is_empty() ? "./" : directory);

  return is_success();
}

bool FileSystemGroup::archive(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      archive,
      "creates a self extracting archive (must have 7z available on the path).")
      + GROUP_ARG_REQ(
        source_path,
        string,
        <path>,
        "path to a directory to archive.")
      + GROUP_ARG_REQ(
        destination_dest,
        string,
        <path>,
        "path to a directory to archive.")
      + GROUP_ARG_OPT(
        encrypt_secure,
        bool,
        false,
        "encrypt the archive using AES256 with a random key (will be "
        "displayed).")
      + GROUP_ARG_OPT(
        extract_decrypt,
        bool,
        false,
        "used to extract an encrypted archive")
      + GROUP_ARG_OPT(
        key,
        string,
        <none>,
        "the key id OR 256-bit encryption keyto use when encrypting/decrypting "
        "the archive")
      + GROUP_ARG_OPT(
        password,
        string,
        <none>,
        "the key password if specifying a key id that requires a password")
      + GROUP_ARG_OPT(
        hash,
        string,
        <none>,
        "hash of the archive that should be decrypted. To skip the hash check, "
        "don't provide a hash")
      + GROUP_ARG_OPT(
        filter_filt,
        string,
        <none>,
        "`?` separated elements that if matched will be excluded."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto source = command.get_argument_value("source");
  const auto destination = command.get_argument_value("destination");
  const auto filter = command.get_argument_value("filter");
  const auto encrypt = command.get_argument_value("encrypt");
  const auto extract = command.get_argument_value("extract");
  const auto hash = command.get_argument_value("hash");
  const auto key = command.get_argument_value("key");
  const auto password = command.get_argument_value("password");

  if (is_cloud_needed_for_key(key)) {
    if (!is_cloud_ready()) {
      printer().close_fail();
    }
  }

  {
    command.print_options(printer());
    SlPrinter::Output printer_output_guard(printer());

    if (!FileSystem().exists(source)) {
      APP_RETURN_ASSIGN_ERROR(source | " path does not exist");
    }

    const auto cipher_key = get_cipher_key(key, password);
    if (cipher_key.string_view().find(":") == 0) {
      APP_RETURN_ASSIGN_ERROR(StringView(cipher_key).pop_front());
    }

    const auto source_info = FileSystem().get_info(source);
    if (source_info.is_file()) {

      const auto destination_path = [&]() -> PathString {
        if (FileSystem().directory_exists(destination)) {
          return destination / Path::name(source);
        }
        return destination;
      }();

      const auto parent_directory = Path::parent_directory(destination_path);
      if (
        !parent_directory.is_empty()
        && !FileSystem().directory_exists(parent_directory)) {
        APP_RETURN_ASSIGN_ERROR(
          "destination " | destination | " cannot be created");
      }

      if (extract == "true") {
        // create a plain file
        sos::Auth::create_plain_file(
          sos::Auth::CreatePlainFile()
            .set_key(cipher_key)
            .set_input_path(source)
            .set_progress_callback(printer().progress_callback())
            .set_output_path(destination_path));

      } else {
        // create a secure file
        sos::Auth::create_secure_file(
          sos::Auth::CreateSecureFile()
            .set_remove_key((encrypt == "true") || !cipher_key.is_empty())
            .set_key(cipher_key)
            .set_input_path(source)
            .set_progress_callback(printer().progress_callback())
            .set_output_path(destination_path));
      }
    }

    // is source is a directory
    if (source_info.is_directory()) {

      if (extract == "true") {

        if (!FileSystem().directory_exists(destination)) {
          APP_RETURN_ASSIGN_ERROR(
            "destination directory " | destination | " does not exist");
        }

        Packager().deploy(
          Packager::DeployOptions()
            .set_archive_path(source)
            .set_key(cipher_key)
            .set_destination_directory_path(destination)
            .set_hash(hash)
            .set_progress_callback(printer().progress_callback())
            .set_download(false));

      } else {

        SL_PRINTER_TRACE("cipher key is " | cipher_key);
        printer().open_object(destination);
        auto packager = std::move(Packager().publish(
          Packager::PublishOptions()
            .set_progress_callback(printer().progress_callback())
            .set_sblob((encrypt == "true") || !cipher_key.is_empty())
            .set_archive_name("archive")
            .set_key(cipher_key)
            .set_remove_key((encrypt == "true") || !cipher_key.is_empty())
            .set_upload(false)
            .set_destination(destination)
            .set_filter(filter)
            .set_source_path(source)));

        if (encrypt == "true") {
          printer()
            .output()
            .key("key256", cipher_key)
            .key(
              "sha256",
              var::View(packager.hash()).to_string<GeneralString>());
        }
      }
    }
  }

  return is_success();
}

bool FileSystemGroup::bundle(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(bundle, "create a binary blob of assets.")
      + GROUP_ARG_REQ(
        source_path,
        string,
        <none>,
        "source file/directory to convert.")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <none>,
        "destination file/directory.")
      + GROUP_ARG_OPT(
        encrypt_secure,
        bool,
        false,
        "encrypt the archive using AES256 with a random key (will be "
        "displayed).")
      + GROUP_ARG_OPT(
        decrypt,
        bool,
        false,
        "used to decrypt an encrypted archive")
      + GROUP_ARG_OPT(
        key,
        bool,
        <auto>,
        "Encryption/decryption key to use (if none is specified, one will be "
        "generated for encryption operations)")
      + GROUP_ARG_OPT(
        access,
        string,
        0444,
        "use a read-only combo (default is 0444)")
      + GROUP_ARG_OPT(
        sourcecode_c,
        bool,
        false,
        "output a `.h` and `.c` file that can be compiled into a project.")
      + GROUP_ARG_OPT(owner, string, root, "specify `user` or `root`."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto decrypt = command.get_argument_value("decrypt");

  command.print_options(printer());

  SlPrinter::Output printer_output_guard(printer());

  if (decrypt == "true") {
    return decrypt_assets(command);
  }

  return gather_assets(command);
}

bool FileSystemGroup::decrypt_assets(const Command &command) {
  const auto source = command.get_argument_value("source");
  const auto destination = command.get_argument_value("destination");
  const auto encrypt = command.get_argument_value("encrypt");
  const auto extract = command.get_argument_value("extract");
  const auto key = command.get_argument_value("key");

  const auto is_secure = Path::suffix(source) == "sassetfs";

  if (!is_secure) {
    APP_RETURN_ASSIGN_ERROR(
      "Input file `" | source | "` is not an `sassetfs` file");
  }

  if (key.is_empty()) {
    APP_RETURN_ASSIGN_ERROR("`key` must be specified to decrypt the assets.");
  }

  // does destination exist?
  const auto output_path = [&]() {
    if (FileSystem().directory_exists(destination)) {
      return destination / Path::base_name(source) & ".assetfs";
    }

    const auto parent_path = Path::parent_directory(destination);
    if (!FileSystem().directory_exists(parent_path)) {
      return PathString();
    }

    if (Path::suffix(destination) != "assetfs") {
      return PathString();
    }

    return PathString(destination);
  }();

  if (destination.is_empty()) {
    APP_RETURN_ASSIGN_ERROR(
      "`destination` should be an existing directory or a `.assetfs` file");
  }

  printer()
    .output()
    .open_object(source)
    .key("plainFile", output_path)
    .close_object();

  sos::Auth::create_plain_file(sos::Auth::CreatePlainFile()
                                 .set_key(key)
                                 .set_input_path(source)
                                 .set_output_path(output_path));
  return true;
}

bool FileSystemGroup::gather_assets(const Command &command) {

  const auto asset_path = command.get_argument_value("source");
  const auto destination = command.get_argument_value("destination");
  const auto access = command.get_argument_value("access");
  const auto owner = command.get_argument_value("owner");
  const auto sourcecode = command.get_argument_value("sourcecode");
  const auto encrypt = command.get_argument_value("encrypt");
  const auto extract = command.get_argument_value("extract");
  const auto key = command.get_argument_value("key");

  JsonKeyValueList<AssetInfo> asset_info_list;

  if (asset_path == "true") {
    // load assets from workspace
    asset_info_list = workspace_settings().get_asset_list();
  } else if (Path::suffix(asset_path) == "json") {
    // load assets from provided file
    WorkspaceSettings tmp_settings;
    tmp_settings.to_object().insert(
      "assets",
      JsonDocument().load(File(asset_path)));
    asset_info_list = tmp_settings.get_asset_list();
  } else {
    // assets are in the target directory
    StringViewList list = asset_path.split("?");
    asset_info_list.push_back(
      AssetInfo(asset_path).set_destination(destination).set_source_list(list));
  }

  for (const AssetInfo &info : asset_info_list) {
    const PathString asset_destination
      = sanitize_asset_destination(info.get_destination());
    DataFile assets_file(OpenMode::read_write().set_append());

    Printer::Object po(printer().active_printer(), asset_destination);
    auto source_list = info.get_source_list();

    class AssetEntry {
    public:
      bool operator<(const AssetEntry &a) const {
        return asset_path().string_view() < a.asset_path().string_view();
      }

    private:
      API_AC(AssetEntry, PathString, host_path);
      API_AC(AssetEntry, PathString, asset_path);
    };

    var::Vector<AssetEntry> asset_list;

    for (const auto &source : source_list) {
      FileInfo source_info = FileSystem().get_info(source);
      if (source_info.is_directory()) {
        auto entry_list
          = FileSystem().read_directory(source, FileSystem::IsRecursive::yes);
        for (const auto &entry : entry_list) {
          if (Path::name(entry) != ".DS_Store") {
            const auto host_path = PathString(source) / entry;
            asset_list.push_back(
              AssetEntry().set_host_path(host_path).set_asset_path(
                host_path.string_view().pop_front(source.length() + 1)));
          }
        }
      } else {
        asset_list.push_back(AssetEntry().set_host_path(source).set_asset_path(
          Path::name(source)));
      }
    }

    asset_list.sort(var::Vector<AssetEntry>::ascending);

    SL_PRINTER_TRACE(
      "gathering " + NumberString(asset_list.count()) + " assets");

    if (asset_list.count() > 0) {
      const u32 count = asset_list.count();
      assets_file.write(View(count));

      // add header table
      u32 start = sizeof(drive_assetfs_dirent_t) * asset_list.count()
                  + sizeof(count); // 4 is the count

      for (const auto &asset : asset_list) {
        SL_PRINTER_TRACE("gathering " + asset.host_path() + " for header");
        const auto file_info = FileSystem().get_info(asset.host_path());
        drive_assetfs_dirent_t dirent = {};
        View(dirent.name).fill(0).copy(View(asset.asset_path().string_view()));
        dirent.start = start;
        dirent.size = file_info.size();
        dirent.mode = access.to_unsigned_long(String::Base::octal);
        dirent.uid = (owner == "user") ? SYSFS_USER : SYSFS_ROOT;
        assets_file.write(View(dirent));

        SL_PRINTER_TRACE(
          "asset " + asset.host_path() + " starts at "
          + NumberString(dirent.start) + " size " + NumberString(dirent.size));

        start += (((dirent.size) & ~0x03) + 4); // align at 4 byte boundary
      }

      for (const auto &asset : asset_list) {
        SL_PRINTER_TRACE(
          "loading " + asset.host_path() + " to write to output");
        assets_file.write(File(asset.host_path()));
        const int size = return_value();
        int padding = (size & ~0x03) + 4 - size;
        SL_PRINTER_TRACE("adding padding " + NumberString(padding));
        Data padding_data(padding);
        View(padding_data).fill<u8>(0);
        assets_file.write(padding_data);
        printer().key(asset.host_path(), NumberString(size).string_view());
      }
    }

    SL_PRINTER_TRACE("creating assets at " + asset_destination);
    File(File::IsOverwrite::yes, asset_destination).write(assets_file.seek(0));

    if (encrypt == "true") {
      crypto::Aes::Key encryption_key;

      if (!key.is_empty()) {
        if (key.length() != 32 && key.length() != 64) {
          APP_RETURN_ASSIGN_ERROR(
            "`key` length must be 32 characters or 64 characters.");
        }
        const auto option_key = crypto::Aes::Key::from_string(key);
        encryption_key.set_key(option_key.get_key256());
      }

      printer().key("key", encryption_key.get_key256_string());

      sos::Auth::create_secure_file(
        sos::Auth::CreateSecureFile()
          .set_padding_character(0)
          .set_key(encryption_key.get_key256_string())
          .set_input_path(asset_destination)
          .set_progress_callback(printer().progress_callback())
          .set_output_path(Path::no_suffix(asset_destination) & ".sassetfs"));
    }

    if (sourcecode == "true") {
      // create .h and .c output files too
      const auto name = Path::base_name(asset_destination);
      const auto parent = Path::parent_directory(asset_destination);
      const auto source_file = parent / name & "_assetfs.c";

      File c_file
        = File(File::IsOverwrite::yes, source_file)
            .write(("const char " | name | "_assetfs[] = {\n").string_view())
            .move();

      var::GeneralString line;
      const auto &data = assets_file.data();
      for (auto offset : api::Index(data.size())) {
        if (offset % 16 == 0) {
          c_file.write(line.append("\n").string_view());
          line = "";
        }
        line |= (NumberString(data.data_u8()[offset], "0x%02X,").string_view());
      }

      c_file.write(line.pop_back().append("\n};\n\n").string_view());
    }
  }

  return true;
}

PathString
FileSystemGroup::sanitize_asset_destination(const var::StringView value) {

  PathString result = value;

  API_RETURN_VALUE_IF_ERROR("");

  if (!FileSystem().exists(result)) {
    const auto parent_directory = Path::parent_directory(result);
    if (
      !parent_directory.is_empty()
      && !FileSystem().exists(Path::parent_directory(result))) {
      API_RETURN_VALUE_ASSIGN_ERROR(
        result,
        ("can't create " + result).cstring(),
        user_error_code());
    }
  }

  FileInfo info = FileSystem().get_info(result);
  if (is_error()) {
    API_RESET_ERROR();
  }

  if (info.is_directory()) {
    if (result.back() != '/') {
      result.append("/");
    }
    result.append("assets");
  }

  if (Path::suffix(result) != "assetfs") {
    result.append(".assetfs");
  }

  return result;
}

var::GeneralString FileSystemGroup::get_cipher_key(
  const StringView key,
  const StringView password) {
  if (key.is_empty()) {
    return "";
  }

  if (key.length() == 64) {
    // this is a 256 bit key
    return key;
  }

  const auto is_json = Path::suffix(key) == "json";
  if (is_json && !FileSystem().exists(key)) {
    return ":" | key | " file could not be found";
  }

  api::ErrorScope error_scope;
  Keys keys = [&]() {
    if (is_json) {
      return Keys().import_file(File(key));
    }
    return Keys(key);
  }();
  if (keys.is_password_required() && password.is_empty()) {
    return ":this key requires a password or is invalid";
  } else {
    SL_PRINTER_TRACE("password is not required OR is provided for key " | key);
  }

  if (is_error()) {
    return ":failed to get specified key";
  } else {
    SL_PRINTER_TRACE("key was downloaded successfully");
  }

  if (!password.is_empty() && password.length() != 64) {
    return ":password is invalid (doesn't have 64 characters)";
  }

  crypto::Aes::Key aes_key
    = password.is_empty()
        ? crypto::Aes::Key().nullify()
        : crypto::Aes::Key::from_string(password, keys.get_iv());

  SL_PRINTER_TRACE("encrypted key is " | keys.get_secure_key_256());

  return View(keys.get_key256(aes_key)).to_string<GeneralString>();
}
