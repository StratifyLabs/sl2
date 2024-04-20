#ifndef TARGETEXECUTIONCONTEXT_HPP
#define TARGETEXECUTIONCONTEXT_HPP

#include "../App.hpp"
#include "../groups/DebugTrace.hpp"
#include "../groups/Task.hpp"
#include "../groups/Terminal.hpp"

class DeviceExecutionContext : public AppAccess {
public:
  class Options {
    API_ACCESS_COMPOUND(Options, var::String, application);
    API_ACCESS_COMPOUND(Options, var::String, arguments);
  };

  DeviceExecutionContext();

  bool execute(const Options &options);
  bool start(const Options &options);
  bool wait(const Options &options);

private:
  API_ACCESS_COMPOUND(DeviceExecutionContext, chrono::MicroTime, duration);
  API_ACCESS_COMPOUND(DeviceExecutionContext, chrono::MicroTime, update_period);
  API_ACCESS_BOOL(DeviceExecutionContext, debug, false);
  API_ACCESS_BOOL(DeviceExecutionContext, analyze, false);
  API_ACCESS_BOOL(DeviceExecutionContext, terminal, true);
  API_ACCESS_BOOL(DeviceExecutionContext, show_terminal, true);
  API_READ_ACCESS_COMPOUND(
    DeviceExecutionContext,
    var::String,
    terminal_output);

  Terminal m_terminal;
  Task m_task;
  DebugTrace m_debug;
};

#endif // TARGETEXECUTIONCONTEXT_HPP
