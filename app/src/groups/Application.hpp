// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "Connector.hpp"
#include "Terminal.hpp"
#include "settings/TestSettings.hpp"

class Application : public Connector {
public:
  explicit Application(Terminal &terminal);

  static void kill(Connection *connection, const StringView name);

private:
  enum commands {
    command_install,
    command_run,
    command_publish,
    command_clean,
    command_ping,
    command_profile,
    command_total
  };

  Terminal &m_terminal;

  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;
  bool install(const Command &command);
  bool run(const Command &command);
  bool publish(const Command &command);
  bool clean(const Command &command);
  bool ping(const Command &command);
  bool profile(const Command &command);
  void clean_directory(const StringView path);

  bool configure_test(
    const var::String &path,
    const TestSettings &test_settings,
    const TestDetails &test_details);
};

#endif // APPLICATION_HPP
