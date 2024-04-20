// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved

#include <fs/Dir.hpp> //first

#include <chrono.hpp>
#include <crypto.hpp>
#include <cstdio>
#include <printer/YamlPrinter.hpp>
#include <sys.hpp>
#include <sys/Cli.hpp>
#include <thread/Mutex.hpp>
#include <thread/Signal.hpp>
#include <thread/Thread.hpp>
#include <usb/Session.hpp>
#include <usb/usb_link_transport_driver.h>
#include <var.hpp>

#include "groups/Admin.hpp"
#include "groups/Application.hpp"
#include "groups/Bench.hpp"
#include "groups/Bsp.hpp"
#include "groups/Cloud.hpp"
#include "groups/Connection.hpp"
#include "groups/DebugTrace.hpp"
#include "groups/FileSystem.hpp"
#include "groups/HardwareGroup.hpp"
#include "groups/KeysGroup.hpp"
#include "groups/Mcu.hpp"
#include "groups/Report.hpp"
#include "groups/Settings.hpp"
#include "groups/Task.hpp"
#include "groups/TeamGroup.hpp"
#include "groups/Terminal.hpp"
#include "groups/Thing.hpp"
#include "groups/UserGroup.hpp"
#include "groups/devices/Flash.hpp"

#include "settings/Credentials.hpp"
#include "settings/GlobalSettings.hpp"

#include "utilities/LocalServer.hpp"

static volatile bool m_is_interrupted = false;

void sigint_handler_function(int a) {
  MCU_UNUSED_ARGUMENT(a);
  m_is_interrupted = true;
  AppAccess::session_settings().set_interrupted(true);
}

void segfault(int a) {
  MCU_UNUSED_ARGUMENT(a);
  static int count = 0;
  if (count == 0) {
    API_ASSERT(false);
    count++;
  }
}

int main(int argc, char *argv[]) {
  signal(11, segfault);
  // signal(6, segfault);

  APP_CALL_GRAPH_TRACE_FUNCTION();

  sys::Cli cli(argc, argv);
  cli(Cli::HandleVersion()
        .set_githash(SOS_GIT_HASH)
        .set_publisher("Stratify Labs, Inc")
        .set_version(VERSION));

  int result = 0;
  Connection connection;
  Connector::set_connection(&connection);

  App::session_settings().set_invoked_as_path(String(argv[0]));

  { // do not remove -- controls constructor/deconstructor execution
    // these need to be deconstructed before being disconnected
    Bsp bsp;
    FileSystemGroup fs;
    Terminal terminal;
#if __admin_release
    Admin admin;
    Mcu mcu;
#endif
    Application application(terminal);
    Task task;
    CloudGroup cloud;
    ThingGroup thing;
    TeamGroup team;
    UserGroup user;
    FlashGroup flash;
    HardwareGroup hardware;
    KeysGroup keys;
    ReportGroup report;
    Settings settings;
    DebugTrace debug_trace(terminal);
    Bench bench;

    App::initialize(cli);

    if (api::ExecutionContext::is_error()) {
      AppAccess::printer().active_printer() << api::ExecutionContext::error();
      exit(1);
    }

#if __admin_release
    Group::add_group(admin);
    Group::add_group(mcu);
#endif
    Group::add_group(application);
    Group::add_group(bench);
    Group::add_group(cloud);
    Group::add_group(connection);
    Group::add_group(debug_trace);
    Group::add_group(fs);
    Group::add_group(bsp);
    Group::add_group(hardware);
    Group::add_group(flash);
    Group::add_group(keys);
    Group::add_group(settings);
    Group::add_group(report);
    Group::add_group(thing);
    Group::add_group(task);
    Group::add_group(terminal);
    Group::add_group(user);
    Group::add_group(team);

    connection.initialize();

    if (Group::execute_cli_arguments(cli) == false) {
      PRINTER_TRACE(Group::printer().output(), "exiting with result code 1");
      exit(1);
    }

    PRINTER_TRACE(
      Group::printer().output(),
      "command line processing complete");

    const bool is_listening = Group::session_settings().is_listen_mode();

    LocalServer local_server;
    if (App::session_settings().is_listen_mode()) {
      local_server.set_web_path(App::session_settings().web_path())
        .set_port(App::session_settings().listen_port())
        .start();
    }

    // message for debug tracing
    if (
      terminal.is_running() || task.is_running() || debug_trace.is_running()
      || is_listening) {
      bool is_terminal = false;

      volatile bool is_busy = true;

      if (connection.is_connected_and_is_not_bootloader()) {
        debug_trace.initialize();
        bool is_task_initialized = task.is_initialized();
        task.initialize();
        task.set_initialized(is_task_initialized);
      }

      if (terminal.is_running()) {
        Group::printer().open_terminal_output();
        printf("\n");
        is_terminal = true;
      }

      MicroTime minimum_update_period = 10000_seconds;

      if (
        terminal.is_initialized()
        && (terminal.update_period() < minimum_update_period)) {
        minimum_update_period = terminal.update_period();
      }

      if (
        task.is_initialized()
        && (task.update_period() < minimum_update_period)) {
        minimum_update_period = task.update_period();
      }

      if (
        debug_trace.is_initialized()
        && (debug_trace.update_period() < minimum_update_period)) {
        minimum_update_period = debug_trace.update_period();
      }

#if 1
      thread::Signal sigint(thread::Signal::Number::interrupt);
      sigint.set_handler(thread::SignalHandler(
        thread::SignalHandler::Construct().set_signal_function(
          sigint_handler_function)));
#endif

      do {

        // terminal can act as a master off switch
        if (terminal.is_initialized() && !terminal.is_running()) {
          task.stop_timers();
          debug_trace.stop_timers();
        }

        is_busy = is_listening;
        is_busy |= terminal.update();
        is_busy |= task.update();
        is_busy |= debug_trace.update();

        wait(minimum_update_period);

        // check to see if the connection failed -- exit if it did

      } while (is_busy && !m_is_interrupted);

      if (!Group::printer().is_vanilla()) {
        Group::printer().output().clear_color_code();
      }

      if (is_terminal) {
        terminal.process_input();
        Group::printer().close_terminal_output();
      }

      terminal.finalize();
      debug_trace.finalize();
      task.finalize();
    }

    // need to save the settings before other objects are destroyed
    App::finalize();

    if (AppAccess::session_settings().is_interrupted()) {
      Group::printer().key("exit", String("SIGINT"));
      Group::printer().insert_newline();
    }

    if (connection.is_connected()) {
      connection.disconnect();
    }
  }
  return result;
}
