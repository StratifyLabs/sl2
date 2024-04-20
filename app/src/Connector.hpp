// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef CONNECTOR_HPP
#define CONNECTOR_HPP

#include "Group.hpp"
#include "groups/Connection.hpp"

class Connector : public Group {
public:
  Connector(const char *name, const char *shortcut);

  Installer::Install get_installer_options(const Command &command);

  Connection *connection() { return m_connection; }
  const Connection *connection() const { return m_connection; }

  static void set_connection(Connection *connection) {
    m_connection = connection;
  }

  bool is_connection_ready(
    IsBootloaderOk is_bootloader_ok = IsBootloaderOk(false),
    IsSuppressError is_suppress_error = IsSuppressError(false));

  bool publish_project(const Command &command, Build::Type project_type);

private:
  static Connection *m_connection;

  void add_session_connection_tags();
};

#endif // CONNECTOR_HPP
