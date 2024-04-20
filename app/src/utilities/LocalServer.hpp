#ifndef UTILITIES_LOCALSERVER_HPP
#define UTILITIES_LOCALSERVER_HPP

#include <inet/Http.hpp>
#include <thread.hpp>

#include "Connector.hpp"

class LocalServer : public AppAccess {
public:
  LocalServer();

  LocalServer &start();

private:
  API_AF(LocalServer, u16, port, 3000);
  API_AC(LocalServer, var::PathString, web_path);

  Thread m_listen_thread;
  var::Array<Thread, 16> m_thread_array;
  inet::Socket m_accept_socket;
  inet::Socket m_server_listen_socket;
  Mutex m_connection_mutex;
  Mutex m_terminal_mutex;
  Mutex m_task_mutex;
  Mutex m_debug_mutex;
  Mutex m_accept_socket_mutex;


  inet::Http::IsStop
  respond(inet::HttpServer *server, const inet::Http::Request &request);

  class MonitorSocketThread {
    API_AF(MonitorSocketThread, LocalServer *, self, nullptr);
    API_AF(MonitorSocketThread, inet::HttpServer *, server, nullptr);
    API_AF(MonitorSocketThread, bool, socket_status, true);

    thread::Mutex m_socket_status_mutex;

  public:

    thread::Mutex & socket_status_mutex() {
      return m_socket_status_mutex;
    }

  };

  void respond_post_command(inet::HttpServer *server);
  void respond_get(inet::HttpServer *server, const var::StringView path);
  void respond_get_web(inet::HttpServer *server, const var::StringView path);
  void respond_terminal(inet::HttpServer *server);
  void respond_task(inet::HttpServer *server);
  void respond_debug(inet::HttpServer *server);
};

#endif // UTILITIES_LOCALSERVER_HPP
