#include "groups/DebugTrace.hpp"
#include "groups/Task.hpp"
#include "groups/Terminal.hpp"

#include "LocalServer.hpp"

#define SERIAL_REQUESTS_ONLY 1

LocalServer::LocalServer() {}

LocalServer &LocalServer::start() {
  {
    printer().open_command("listen");
    SlPrinter::Output printer_output_guard(printer());
    printer().key("port", NumberString(port()));
  }

  m_listen_thread = Thread(
    [this]() -> void * {

      inet::AddressInfo address_info(
        inet::AddressInfo::Construct()
          .set_family(inet::Socket::Family::inet)
          .set_service(NumberString(port()))
          .set_type(inet::Socket::Type::stream)
          .set_flags(inet::AddressInfo::Flags::passive));

      const inet::SocketAddress &server_listen_address
        = address_info.list().at(0);

      inet::Socket server_listen_socket
        = inet::Socket(server_listen_address)
            .set_option(inet::SocketOption(
              inet::Socket::Level::socket,
              inet::Socket::NameFlags::socket_reuse_address))
            .bind_and_listen(server_listen_address)
            .move();

      while (session_settings().is_interrupted() == false) {

        {
          inet::SocketAddress accept_address;
          // this will block until a connection arrives
          m_accept_socket = server_listen_socket.accept(accept_address);
        }

        // start the server in a new thread
        for (auto &thread : m_thread_array) {
          if (!thread.is_valid() || !thread.is_running()) {
            thread =  Thread([this]() -> void * {
                  inet::Socket accept_socket = std::move(m_accept_socket);

#if SERIAL_REQUESTS_ONLY
                  Mutex::Guard mg(m_accept_socket_mutex);
#endif
                  // tell the listener thread to move on
                  //self->m_sync_mutex.lock_synced();
                  //inet::HttpServer(accept_socket.move()).run(self, respond);
                  inet::HttpServer(accept_socket.move())
                    .run([this](HttpServer * server, const inet::Http::Request &request) {
                      return respond(server, request);
                    });
                  return nullptr;
                });

            //self->m_sync_mutex.unlock_synced();

#if SERIAL_REQUESTS_ONLY
            Mutex::Guard mg(m_accept_socket_mutex);
#endif

            break;
          }
        }
        if (is_error()) {
          printer().output().object("listen thread error", error());
        }
        API_RESET_ERROR();
      }

      return nullptr;
    });

  return *this;
}

inet::Http::IsStop LocalServer::respond(
  inet::HttpServer *server,
  const inet::Http::Request &request) {

  if (request.method() == inet::Http::Method::post) {
    if (request.path() == "/command") {
      respond_post_command(server);
    } else {
      DataFile incoming = DataFile().reserve(4096).move();
      server->receive(incoming)
        .add_header_field("Content-Length", "0")
        .send(inet::Http::Response(
          server->http_version(),
          inet::Http::Status::bad_request));
    }
  } else if (request.method() == inet::Http::Method::get) {
    respond_get(server, request.path());
#if SERIAL_REQUESTS_ONLY
    return inet::Http::IsStop::yes;
#endif
  } else if (request.method() == inet::Http::Method::options) {
    server->add_header_field("Content-Type", "text/event-stream")
      .add_header_field("Connection", "keep-alive")
      .add_header_field("Access-Control-Allow-Origin", "*")
      .add_header_field("Access-Control-Request-Method", "GET,POST,OPTIONS")
      .add_header_field(
        "Access-Control-Request-Headers",
        "X-PINGOTHER, Content-Type")
      .send(
        inet::Http::Response(server->http_version(), inet::Http::Status::ok));
  } else if (request.method() == inet::Http::Method::null) {
    return inet::Http::IsStop::yes;
  }

  return inet::Http::IsStop::no;
}

void LocalServer::respond_get(inet::HttpServer *server, const StringView path) {

  // check for server sent events request
  if (path == "/terminal") {
    // send terminal data as server side events
    printf("respond to terminal\n");
    respond_terminal(server);
    printf("respond terminal complete\n");
  } else {
    respond_get_web(server, path);
  }
}

void LocalServer::respond_get_web(
  inet::HttpServer *server,
  const var::StringView path) {
  // map requests to /assets/web
  const bool is_home = (path == "/");

  const PathString local_path
    = web_path() & path & (is_home ? "index.html" : "");

  const bool is_non_gz = FileSystem().exists(local_path);
  const bool is_gz = FileSystem().exists(local_path & ".gz");
  if (!is_non_gz && !is_gz) {
    // return bad request
    server->add_header_field("Connection", "close")
      .send(inet::Http::Response(
        server->http_version(),
        inet::Http::Status::bad_request));
    return;
  }

  if (web_path().is_empty()) {
    server->add_header_field("Connection", "close")
      .send(inet::Http::Response(
        server->http_version(),
        inet::Http::Status::bad_request));
    return;
  }

  const File local_file(local_path & (is_gz ? ".gz" : ""));

  printf("send local file %s\n", local_path.cstring());
  if (is_gz) {
    // need to check if incoming request allows for gz
    server->add_header_field("Content-Encoding", "gzip");
  }

  server->add_header_field("Content-Length", NumberString(local_file.size()))
    .add_header_field("Cache-Control", "public,max-age=604800")
    .send(inet::Http::Response(server->http_version(), inet::Http::Status::ok));

  server->socket().write(
    local_file,
    inet::Socket::Write().set_page_size(3000).set_progress_callback(
      printer().progress_callback()));

  server->set_running(false);
}

void LocalServer::respond_terminal(inet::HttpServer *server) {
  Terminal *terminal
    = reinterpret_cast<Terminal *>(Group::get_group("terminal"));
  API_ASSERT(terminal != nullptr);

  if (m_terminal_mutex.try_lock() == false) {
    server->add_header_field("Connection", "close")
      .send(inet::Http::Response(
        server->http_version(),
        inet::Http::Status::conflict));
    return;
  }

  server->add_header_field("Content-Type", "text/event-stream")
    .add_header_field("Connection", "keep-alive")
    .add_header_field("Access-Control-Allow-Origin", "*")
    .add_header_field("Access-Control-Request-Method", "GET,POST,OPTIONS")
    .send(inet::Http::Response(server->http_version(), inet::Http::Status::ok));

  MonitorSocketThread monitor_socket_thread
    = std::move(MonitorSocketThread().set_self(this).set_server(server));

  // we need to listen to the socket and see if the connection closes
  Thread listen_socket_thread = Thread(
    Thread::Attributes().set_detach_state(Thread::DetachState::joinable),
    Thread::Construct()
      .set_argument(&monitor_socket_thread)
      .set_function([](void *args) -> void * {
        MonitorSocketThread *monitor_socket_thread
          = reinterpret_cast<MonitorSocketThread *>(args);
        char a;
        monitor_socket_thread->server()->socket().read(View(a));
        {
          Mutex::Guard mg(monitor_socket_thread->socket_status_mutex());
          monitor_socket_thread->set_socket_status(false);
        }

        return nullptr;
      }));

  bool is_socket_open = true;
  // get header should be looking for server side events
  while (is_socket_open) {
    while (terminal->queue_count()) {

      var::String line;
      line = "data: " + terminal->pop_queue();

      // encode newlines with `data: `
      line(String::Replace().set_old_string("\n").set_new_string(
        "<newline>data: "))(
        String::Replace().set_old_string("<newline>").set_new_string("\n"));

      line += "\n\n";

      server->send(ViewFile(line));
    }

    {
      Mutex::Guard mg(monitor_socket_thread.socket_status_mutex());
      is_socket_open = monitor_socket_thread.socket_status();
    }

    wait(100_milliseconds);
  }

  listen_socket_thread.join();
  m_terminal_mutex.unlock();
}

void LocalServer::respond_task(inet::HttpServer *server) {
  Task *task = reinterpret_cast<Task *>(Group::get_group("task"));
  API_ASSERT(task != nullptr);

  if (m_task_mutex.try_lock() == false) {
    server->add_header_field("Connection", "close")
      .send(inet::Http::Response(
        server->http_version(),
        inet::Http::Status::conflict));
    return;
  }

  // get header should be looking for server side events
  while (is_success()) {
    if (task->queue_count()) {

      var::String line;
      line = "data: " + task->pop_queue();

      server->send(ViewFile(line));
    }
    wait(100_milliseconds);
  }

  m_task_mutex.unlock();
}

void LocalServer::respond_debug(inet::HttpServer *server) {}

void LocalServer::respond_post_command(inet::HttpServer *self) {
  DataFile incoming = DataFile().reserve(4096).move();
  // process REST API requests
  self->receive(incoming);
  JsonValue value = JsonDocument().load(incoming.seek(0));

  if (value.is_valid() == false) {
    self->send(inet::Http::Response(
      self->http_version(),
      inet::Http::Status::bad_request));
    return;
  }

  var::Vector<Group::CommandInput> list;
  if (value.is_object()) {
    list.push_back(Group::CommandInput(Command::stringify(value.to_object())));
  } else if (value.is_array()) {
    list.reserve(value.to_array().count());
    for (size_t i = 0; i < value.to_array().count(); i++) {
      list.push_back(Group::CommandInput(
        Command::stringify(value.to_array().at(i).to_object())));
    }
  } else {
    self->send(inet::Http::Response(
      self->http_version(),
      inet::Http::Status::bad_request));
    return;
  }

  printer().insert_newline();
  String reply;
  reply.reserve(4096);
  {
    Mutex::Guard mg(m_connection_mutex);
    Group::execute_command_list(list);

    while (printer().queue().count() > 0) {
      reply += printer().queue().front();
      printer().queue().pop();
    }
  }

  if (printer().is_json()) {

    if (reply.length() && reply.front() == ',') {
      reply.pop_front();
    }

    reply = "[" + reply + "]\n\n";
  }

  // self->set_header_fields();
  self->add_header_field("Content-Length", NumberString(reply.length()))
    .add_header_field("Connection", "close")
    .add_header_field(
      "Content-Type",
      printer().is_json() ? "application/json" : "application/text")
    .send(inet::Http::Response(self->http_version(), inet::Http::Status::ok))
    .send(ViewFile(reply));
}
