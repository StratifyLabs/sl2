#include <fs.hpp>
#include <sos.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "Report.hpp"

ReportGroup::ReportGroup() : Connector("report", "report") {}

var::StringViewList ReportGroup::get_command_list() const {
  StringViewList list
    = {"publish", "list", "remove", "ping", "parse", "download"};
  API_ASSERT(list.count() == command_total);
  return list;
}

bool ReportGroup::execute_command_at(u32 list_offset, const Command &command) {
  switch (list_offset) {
  case command_publish:
    return publish(command);
  case command_list:
    return list(command);
  case command_remove:
    return remove(command);
  case command_ping:
    return ping(command);
  case command_parse:
    return parse(command);
  case command_download:
    return download(command);
  }
  return false;
}

bool ReportGroup::publish(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      publish,
      "publishes the report (markdown file) specified by the path to either "
      "the user (default) or the team if specified.")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "List what will be uploaded without uploading")
      + GROUP_ARG_OPT(
        team,
        string,
        <none>,
        "team ID for the report (if not specified, report is published "
        "publicly to the user account")
      + GROUP_ARG_OPT(thing, string, <none>, "thing associated with the report")
      + GROUP_ARG_OPT(
        project,
        string,
        <none>,
        "project associated with the report")
      + GROUP_ARG_OPT(
        permissions,
        string,
        public,
        "project associated with the report")
      + GROUP_ARG_OPT(
        tags,
        string,
        <none>,
        "tags for the report (use ? to separate values)")
      + GROUP_ARG_REQ(
        path_p,
        string,
        <markdown file path>,
        "Path to the report (markdown file) to publish."));

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  StringView thing = command.get_argument_value("thing");
  StringView permissions = command.get_argument_value("permissions");
  StringView tags = command.get_argument_value("tags");
  StringView path = command.get_argument_value("path");
  StringView team = command.get_argument_value("team");
  StringView project = command.get_argument_value("project");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  Link::Path link_path(path, connection()->driver());

  Link::FileSystem file_system(link_path.driver());

  if (link_path.is_device_path()) {
    printer().troubleshoot(
      "Use `host@<relative file path>` to specify a path on the host machine");
    APP_RETURN_ASSIGN_ERROR("File must be on host machine");
  }

  SlPrinter::Output printer_output_guard(printer());

  printer().open_object("report");

  if (Document::is_permissions_valid(permissions) == false) {
    printer().troubleshoot(
      "Valid values for `permissions` include "
      + Document::get_valid_permissions());
    APP_RETURN_ASSIGN_ERROR("permissions value is not valid");
  }

  if (file_system.exists(link_path.path()) == false) {
    APP_RETURN_ASSIGN_ERROR("the file path `" + path + "` does not exist");
  }

  const auto report_name = Path::name(link_path.path());

  if (report_name.is_empty()) {
    APP_RETURN_ASSIGN_ERROR(
      "Report name taken from file path `" + link_path.path() + "` is emptry");
  }

  StringView report_id
    = Report()
        .set_project_id(project)
        .set_tag_list(tags.split("?").convert(StringView::to_string))
        .set_thing_id(thing)
        .set_permissions(permissions)
        .set_team_id(team)
        .set_name(report_name)
        .save(Link::File(
          link_path.path(),
          OpenMode::read_only(),
          link_path.driver()))
        .id();

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("Failed to save the report");
  }

  printer().key("id", report_id);

  return is_success();
}

bool ReportGroup::list(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(publish, "lists the reports.")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "List what will be uploaded without uploading")
      + GROUP_ARG_OPT(
        team,
        string,
        <none>,
        "list reports for the specified team")
      + GROUP_ARG_OPT(
        thing,
        string,
        <none>,
        "list reports associated with thing specified")
      + GROUP_ARG_OPT(
        project,
        string,
        <none>,
        "list reports associated with project specified")
      + GROUP_ARG_OPT(
        tags,
        string,
        <none>,
        "list the reports that match the specified tags"));

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  StringView thing = command.get_argument_value("thing");
  StringView tags = command.get_argument_value("tags");
  StringView team = command.get_argument_value("team");
  StringView project = command.get_argument_value("project");

  command.print_options(printer());

  return is_success();
}

bool ReportGroup::remove(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(publish, "deletes a report.")
      + GROUP_ARG_OPT(
        dryrun,
        bool,
        false,
        "List the report to be deleted without deleting")
      + GROUP_ARG_OPT(team, string, <none>, "team associated with the report")
      + GROUP_ARG_REQ(id, string, <reportId>, "report id to be deleted"));

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  StringView team = command.get_argument_value("team");

  command.print_options(printer());

  return is_success();
}

bool ReportGroup::ping(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(ping, "pings a report.")
      + GROUP_ARG_OPT(
        display,
        bool,
        false,
        "display the contents of the report")
      + GROUP_ARG_OPT(
        encode,
        bool,
        false,
        "display as base64 encoded (allows content to be parsed in output)")
      + GROUP_ARG_REQ(
        identifier_id,
        string,
        <reportId>,
        "report id to be deleted"));

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  StringView identifier = command.get_argument_value("identifier");
  StringView display = command.get_argument_value("display");
  StringView encode = command.get_argument_value("encode");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object(identifier);
  DataFile destination;
  Report report(
    identifier,
    destination,
    display == "true" ? Report::IsDownloadContents::yes
                      : Report::IsDownloadContents::no);

  if (!report.is_existing()) {
    APP_RETURN_ASSIGN_ERROR(
      "failed to download report, please check the identifier");
  }

  printer().object("report", report);

  if (display == "true") {

    const StringView report_string = destination.data().add_null_terminator();

    if (encode == "true") {
      printer().key("contents", Base64().encode(report_string));
    } else {
      printer().close_success();

      printer().open_external_output();
      printer() << report_string;
      printer().close_external_output();
      return true;
    }
  }

  return is_success();
}

bool ReportGroup::parse(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      parse,
      "parses a txt stream and generates a markdown report.")
      + GROUP_ARG_OPT(
        display,
        bool,
        false,
        "display the contents of the report")
      + GROUP_ARG_OPT(
        encode,
        bool,
        false,
        "display as base64 encoded (allows content to be parsed in output)")
      + GROUP_ARG_OPT(
        publish,
        bool,
        false,
        "Publish the output as a new report")
      + GROUP_ARG_OPT(
        permissions,
        string,
        public,
        "Permisssions if publishing a new report")
      + GROUP_ARG_OPT(
        team,
        string,
        <none>,
        "Team if publishing with private permissions")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <none>,
        "Save the parsed output to a file")
      + GROUP_ARG_REQ(
        source_path,
        string,
        <path to txt file>,
        "path to plain text file that will be parsed"));

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  StringView source = command.get_argument_value("source");
  StringView destination = command.get_argument_value("destination");
  StringView display = command.get_argument_value("display");
  StringView encode = command.get_argument_value("encode");
  StringView publish = command.get_argument_value("publish");
  StringView permissions = command.get_argument_value("permissions");
  StringView team = command.get_argument_value("team");

  command.print_options(printer());

  if (publish == "true") {
    if (is_cloud_ready() == false) {
      return printer().close_fail();
    }
  }

  SlPrinter::Output printer_output_guard(printer());

  printer().open_object("report");
  DataFile output_file;
  File file(source);

  printer().key("size", NumberString(output_file.data().size()));

  if (destination.is_empty() == false) {
    File(File::IsOverwrite::yes, destination).write(output_file);
  }

  if (publish == "true") {

    output_file.seek(0);
    service::Report report;
    report.set_tag_list(session_settings().tag_list())
      .set_permissions(permissions)
      .set_name(Path::name(source))
      .set_team_id(team)
      .save(output_file);

    if (is_error()) {
      printer().error("failed to ping report: " + cloud_service().store().error_string());
      return printer().close_fail();
    }

    printer().key("report", report.id());
  }

  if( destination.is_empty() == false ){
    const PathString destination_parent_path
      = Path::parent_directory(destination);

    if( destination_parent_path.is_empty() == false &&
        FileSystem().directory_exists(destination_parent_path) == false ){
      printer().error("cannot create : " | destination | ": parent directory doesn't exist");
      return printer().close_fail();
    }

    File(File::IsOverwrite::yes, destination).write(output_file.seek(0));
  }

  printer().close_success();

  if (display == "true") {
    if (encode == "true") {
      printer().key("base64", Base64().encode(output_file.data()));
    } else {
      printer().open_external_output();
      printf("%s", output_file.data().add_null_terminator());
      printer().close_external_output();
    }
  }


  return true;
}

bool ReportGroup::download(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(ping, "pings a report.")
      + GROUP_ARG_OPT(
        destination_dest,
        string,
        <name of report.md>,
        "Path to the destination file")
      + GROUP_ARG_REQ(
        identifier_id,
        string,
        <reportId>,
        "report id to be deleted"));

  if (command.is_valid(reference, printer()) == false) {
    return printer().close_fail();
  }

  const StringView identifier = command.get_argument_value("identifier");
  PathString destination = command.get_argument_value("destination");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  printer().open_object("report");

  DataFile data_file;
  Report report(identifier, data_file);

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR(
      "failed to download report, please check the identifier");
  }

  if (destination.is_empty()) {
    destination = identifier & StringView(".md");
  }

  File(File::IsOverwrite::yes, destination).write(data_file.seek(0));

  return is_success();
}
