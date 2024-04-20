// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef CLOUD_HPP
#define CLOUD_HPP

#include "Connector.hpp"

class CloudAppUpdate {
public:
  CloudAppUpdate() {}

  CloudAppUpdate(
    const var::PathString &path,
    const var::NameString &build,
    const Project &project,
    const sos::Appfs::Info &info) {
    m_path = path;
    m_project = project;
    m_info = info;
    m_build = build;
  }

  bool is_valid() const { return path().is_empty() == false; }

private:
  API_READ_ACCESS_COMPOUND(CloudAppUpdate, var::PathString, path);
  API_READ_ACCESS_COMPOUND(CloudAppUpdate, var::NameString, build);
  API_READ_ACCESS_COMPOUND(CloudAppUpdate, sos::Appfs::Info, info);
  API_ACCESS_COMPOUND(CloudAppUpdate, Project, project);
};

class CloudGroupOptions {};

class CloudGroup : public Connector {
public:
  CloudGroup();

  static ExecutionStatus publish_job(const var::String &command);

private:
  enum commands {
    command_login,
    command_logout,
    command_refresh,
    command_install,
    command_ping,
    command_sync,
    command_connect,
    command_listen,
    command_remove,
    command_total
  };

  service::Job::Server *m_job_server;

  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool login(const Command &command);
  bool logout(const Command &command);
  bool refresh(const Command &command);
  bool install(const Command &command);
  bool ping(const Command &command);
  bool sync(const Command &command);
  bool connect(const Command &command);
  bool listen(const Command &command);
  bool remove(const Command &command);

  bool is_project_path_valid(const StringView path);

  int refresh(bool force_refresh = false);

  var::Vector<CloudAppUpdate>
  check_for_app_updates(PathString path, IsForceReinstall is_reinstall);

  void apply_app_updates(const CloudAppUpdate &update);
  bool check_for_update();

  static json::JsonValue handle_cloud_listen_function(
    void *context,
    const var::StringView type,
    const json::JsonValue &value) {
    return reinterpret_cast<CloudGroup *>(context)->handle_cloud_listen(
      type,
      value);
  }

  json::JsonValue
  handle_cloud_listen(const var::StringView type, const json::JsonValue &value);
};

#endif // CLOUD_HPP
