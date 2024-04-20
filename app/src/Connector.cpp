// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include <chrono.hpp>
#include <crypto.hpp>
#include <fs.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "Connector.hpp"
#include "settings/FilePathSettings.hpp"

Connection *Connector::m_connection = nullptr;

Connector::Connector(const char *name, const char *shortcut)
  : Group(name, shortcut) {}

void Connector::add_session_connection_tags() {
  APP_CALL_GRAPH_TRACE_FUNCTION();
  const auto serial_number = connection()->info().serial_number().to_string();

  session_settings()
    .add_thing_tag(serial_number)
    .add_team_tag(connection()->info().team_id())
    .tag_list()
    .push_back(String("thing:") + serial_number);

  if (!connection()->info().team_id().is_empty()) {
    session_settings().set_report_permissions(String("private"));
    session_settings().set_report_team(
      connection()->info().team_id().to_string());
  }
}

bool Connector::is_connection_ready(
  IsBootloaderOk is_bootloader_ok,
  IsSuppressError is_suppress_error) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("Connector");

  if (connection()->is_connected_and_is_not_bootloader()) {
    add_session_connection_tags();
    return true;
  }

  printer().open_connecting();

  if (connection()->is_connected() && connection()->is_bootloader()) {
    SL_PRINTER_TRACE("already connected but to a bootloader");
    if (is_bootloader_ok == IsBootloaderOk::yes) {
      return printer().close_connecting_success(
        connection()->info().serial_number().to_string());
    }
    if (is_suppress_error == IsSuppressError::no) {
      printer().error("connected to bootloader");
      printer().troubleshoot(
        "`sl` connected to a bootloader instance but was expecting to connect "
        "to Stratify OS. You can try resetting "
        "the device and if that does not work you will need to install "
        "Stratify OS using the bootloader.");
      printer().print_os_install_options();
    }

  } else {
    // try to connect to the last known connection

    const auto preferred_connection
      = workspace_settings().get_preferred_connection();

    const auto try_connect = [&]() {
      if (!preferred_connection.get_path().is_empty()) {
        return Connection::TryConnect()
          .set_driver_path(sos::Link::DriverPath(preferred_connection.get_path()))
          .set_retry_count(preferred_connection.get_retry())
          .set_delay_interval(preferred_connection.get_delay() * 1_milliseconds);
      }

      return Connection::TryConnect();
    }();

    if (!connection()->try_connect(try_connect)) {
      if (is_suppress_error == IsSuppressError::no) {
        if(!try_connect.driver_path().path().is_empty()){
          printer().error("failed to connect to " | try_connect.driver_path().path());
        } else {
          printer().error("failed to connect to any device");
        }

      }
    } else {
      if (
        connection()->is_bootloader()
        && is_bootloader_ok == IsBootloaderOk::no) {
        if (is_suppress_error == IsSuppressError::no) {
          printer().error("connected to bootloader");
          printer().troubleshoot(
            "`sl` connected to a bootloader instance but was expecting to "
            "connect to Stratify OS. You can try "
            "resetting the device and if that does not work you will need to "
            "install Stratify OS using the "
            "bootloader.");
          printer().print_os_install_options();
        }
      } else {
        printer().connecting().object(
          "connection",
          connection()->info(),
          printer::Printer::Level::message);
        add_session_connection_tags();
        return printer().close_connecting_success(
          connection()->info().serial_number().to_string());
      }
    }
  }

  printer().troubleshoot(
    "`sl` failed to connect to Stratify OS. First, please check the physical "
    "connection and verify the device is "
    "powered and running. Second, perform a hardware reset of the device if "
    "that is available. ");

  if (sys::System::is_windows()) {
    printer().troubleshoot(
      "Third, check the device manager to ensure the USB connection is working "
      "correctly.");
  } else {
    printer().troubleshoot(
      "Third, check the System Information application to ensure the USB "
      "connection is working correctly. You can "
      "search for `System Information` in spotlight to open the application.");
  }

  return printer().close_connecting_fail();
}

Installer::Install Connector::get_installer_options(const Command &command) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("Connector");

  const auto external = command.get_argument_value("external");
  StringView external_code = command.get_argument_value("externalcode");
  StringView external_data = command.get_argument_value("externaldata");

  if (!external.is_empty()) {
    external_code = external;
    external_data = external;
  }

  const StringView tightlycoupled
    = command.get_argument_value("tightlycoupled");
  StringView tightlycoupled_code
    = command.get_argument_value("tightlycoupledcode");
  StringView tightlycoupled_data
    = command.get_argument_value("tightlycoupleddata");

  if (!tightlycoupled.is_empty()) {
    tightlycoupled_code = tightlycoupled;
    tightlycoupled_data = tightlycoupled;
  }

  const auto sign_key_password = [](StringView password) {
    if (password.is_empty()) {
      return StringView(Aes::Key::get_null_key256_string());
    }
    return password;
  }(command.get_argument_value("signkeypassword"));

  // synchronize requires a reconnect
  const auto is_synchronize = [&]() {
    if (command.get_argument_value("synchronize") != "true") {
      return false;
    }
    if (command.get_argument_value("reconnect") != "true") {
      return false;
    }
    return true;
  }();

  return Installer::Install()
    .set_project_id(command.get_argument_value("identifier"))
    .set_team_id(command.get_argument_value("team"))
    .set_public_key_id(command.get_argument_value("publickey"))
    .set_sign_key_id(command.get_argument_value("signkey"))
    .set_default_sign_key_id(workspace_settings().get_sign_key())
    .set_sign_key_password(sign_key_password)
    .set_url(command.get_argument_value("url"))
    .set_version(command.get_argument_value("version"))
    .set_build_name(command.get_argument_value("build"))
    .set_architecture(command.get_argument_value("architecture"))
    .set_destination(command.get_argument_value("destination"))
    .set_suffix(command.get_argument_value("suffix"))
    .set_tightly_coupled_data(tightlycoupled_data == "true")
    .set_tightly_coupled_code(tightlycoupled_code == "true")
    .set_external_data(external_data == "true")
    .set_external_code(external_code == "true")
    .set_clean(command.get_argument_value("clean") == "true")
    .set_force(command.get_argument_value("force") == "true")
    .set_kill(command.get_argument_value("kill") == "true")
    .set_flash_device(command.get_argument_value("flashdevice"))
    .set_flash(command.get_argument_value("ram") != "true")
    .set_orphan(command.get_argument_value("orphan") == "true")
    .set_startup(command.get_argument_value("startup") == "true")
    .set_authenticated(command.get_argument_value("authenticated") == "true")
    .set_ram_size(command.get_argument_value("ramsize").to_unsigned_long())
    .set_access_mode(command.get_argument_value("access").to_unsigned_long())
    .set_verify(command.get_argument_value("verify") == "true")
    .set_application_name(command.get_argument_value("name"))
    .set_append_hash(command.get_argument_value("hash") == "true")
    .set_delay(
      command.get_argument_value("delay").to_unsigned_long() * 1_milliseconds)
    .set_reconnect(command.get_argument_value("reconnect") == "true")
    .set_retry_reconnect_count(
      command.get_argument_value("retry").to_unsigned_long())
    .set_insert_key(command.get_argument_value("key") == "true")
    .set_secret_key(command.get_argument_value("secretkey"))
    .set_synchronize_thing(is_synchronize)
    .set_rekey_thing(command.get_argument_value("rekey") == "true");
}

bool Connector::publish_project(
  const Command &command,
  Build::Type project_type) {
  APP_CALL_GRAPH_TRACE_CLASS_FUNCTION("Connector");

  const StringView path = command.get_argument_value("path");
  const StringView fork = command.get_argument_value("fork");
  const StringView roll = command.get_argument_value("roll");
  const StringView dryrun = command.get_argument_value("dryrun");
  const StringView team = command.get_argument_value("team");
  const StringView sign_key = command.get_argument_value("signkey");
  const auto sign_key_password = [](StringView pass) {
    if (pass.is_empty()) {
      return StringView(Aes::Key::get_null_key256_string());
    }
    return StringView(pass);
  }(command.get_argument_value("signkeypassword"));

  command.print_options(printer());

  if (!is_cloud_ready()) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  PathList list;

  if (path.is_empty()) {
    list = FileSystem().read_directory(".");
  } else {
    if (!is_directory_valid(path)) {
      return false;
    }
    list.push_back(PathString(path));
  }

  for (const StringView project_path : list) {
    SL_PRINTER_TRACE("publish project at " | project_path);
    printer().open_object(project_path);

    Project project = Project().import_file(
      File(FilePathSettings::project_settings_file_path(project_path)));

    if (is_error()) {
      printer().error("project " + project_path + " not found");
      return false;
    }

    if (!team.is_empty() && !project.get_team_id().is_empty()) {
      if (team != project.get_team_id()) {
        printer().error("team was specified but already exists in the project");
        return false;
      }
    }

    Build::Type type = Build::decode_build_type(project.get_type());

    if (type == project_type) {
      // does the type match
      SL_PRINTER_TRACE("project matches type specified");

      if (fork == "true") {
        SL_PRINTER_TRACE("forking previously published project");
        project.set_user_id(cloud_service().cloud().credentials().get_uid())
          .set_version("0.1")
          .remove_build_list()
          .set_team_id(team)
          .set_id("")
          .set_document_id("");
      }

      sys::Version version(project.get_version());
      // roll the version as bcd16
      if (roll == "true" && fork != "true") {
        SL_PRINTER_TRACE(String().format(
          "roll version from 0x%X to 0x%X",
          version.to_bcd16(),
          version.to_bcd16() + 1));
        version
          = sys::Version::from_u16(static_cast<u16>(version.to_bcd16() + 1));
        project.set_version(version.string_view());
      }

      const StringView id = project.get_document_id();

      printer().key("id", id.is_empty() ? StringView("unpublished") : id);
      printer().key("version", version.string_view());

      printer().object(
        "settings",
        project.to_object(),
        printer::Printer::Level::debug);

      // this will import the project and publish the update
      project.save_build(Project::SaveBuild()
                           .set_project_path(project_path)
                           .set_sign_key(sign_key)
                           .set_sign_key_password(sign_key_password));

      if (is_error()) {
        // failed to publish the changes
        SL_PRINTER_TRACE(cloud_service().store().traffic());
        handle_document_cloud_error();
        return false;
      }
      if (dryrun != "true") {
        SL_PRINTER_TRACE("Exporting new project settings");
        project.export_file(File(
          File::IsOverwrite::yes,
          FilePathSettings::project_settings_file_path(project_path)));
      }
    } else {
      if (path != "<all>") {
        printer().error(
          project_path + " of type " + Build::encode_build_type(type)
          + " does not match type "
          + Build::encode_build_type(static_cast<Build::Type>(project_type)));
        return false;
      }
      SL_PRINTER_TRACE(
        "skipping '" + project_path + "' project type doesn't match command");
    }
    printer().close_object();
  }

  return true;
}
