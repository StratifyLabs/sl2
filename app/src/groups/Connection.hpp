// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <sos/Link.hpp>
#include <sos/link/transport.h>
#include <usb/usb_link_transport_driver.h>

#include "../Group.hpp"

class Connection : public sos::Link, public Group {
public:
  Connection();
  int initialize();
  int finalize();
  using sos::Link::connect;

  class TryConnect {
  public:
    TryConnect() { set_delay_interval(500_milliseconds); }

  private:
    API_ACCESS_COMPOUND(TryConnect, sos::Link::DriverPath, driver_path);
    API_ACCESS_FUNDAMENTAL(TryConnect, u32, retry_count, 10);
    API_ACCESS_COMPOUND(TryConnect, chrono::MicroTime, delay_interval);
  };

  bool try_connect(const TryConnect &options);
  bool operator()(TryConnect &options) { return try_connect(options); }

private:
  var::String m_path;
  var::String m_serial_number;

  link_transport_serial_options_t m_serial_options;
  link_transport_mdriver_t m_link_driver;

  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;
  bool list(const Command &command);
  bool ping(const Command &command);
  bool connect(const Command &command);

  enum commands { command_list, command_ping, command_connect, command_total };

  void try_connect_to_serial_number(const TryConnect &options);
  void try_connect_to_path(const TryConnect &options);
  void try_connect_to_any(const TryConnect &options);
  bool is_path_blacklisted(const StringView path);

  var::String
  get_serial_port_path_from_hardware_id(const sos::Link::DriverPath &driver_path);

  PathList create_path_list(const StringView driver_name);
  fs::PathList create_path_list_serial();
  fs::PathList create_path_list_usb();
  fs::PathList create_path_list_windows(const StringView driver_name);
  fs::PathList create_path_list_posix(const var::StringView driver_name);

  void load_driver(const var::StringView path) {
    if (path.find("/usb") == 0 || path.find("@usb") == 0) {
      SL_PRINTER_TRACE("Load USB driver");
      usb_link_transport_load_driver(driver());
    } else {
      SL_PRINTER_TRACE("Load serial driver");
      link_load_default_driver(driver());
    }
  }
};

#endif // CONNECTION_HPP
