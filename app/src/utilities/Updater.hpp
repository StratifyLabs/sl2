// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef UPDATER_HPP
#define UPDATER_HPP

#include "Connector.hpp"
#include <chrono.hpp>

class Updater : public Connector {
public:
  Updater(const char *name, const char *shortcut);

  virtual bool update() = 0;

  size_t queue_count() {
    thread::Mutex::Guard mg(m_queue_mutex);
    return m_queue.count();
  }

  void push_queue(const var::String &value) {
    thread::Mutex::Guard mg(m_queue_mutex);
    m_queue.push(value);
  }

  var::String pop_queue() {
    thread::Mutex::Guard mg(m_queue_mutex);
    if (m_queue.count() == 0) {
      return var::String();
    }
    var::String result = m_queue.front();
    m_queue.pop();
    return result;
  }

  bool is_stopped() const { return m_duration_timer.is_stopped(); }
  bool is_running() const { return m_duration_timer.is_running(); }
  bool is_time_to_update();
  bool is_duration_complete();

  bool is_initialized() const { return m_is_initialized; }
  void set_initialized(bool value = true) { m_is_initialized = value; }

  void start_timers() {
    m_update_timer.start();
    m_duration_timer.start();
    set_initialized();
  }

  void stop_timers() {
    m_update_timer.stop();
    m_duration_timer.stop();
  }

  const chrono::ClockTimer &duration_timer() const { return m_duration_timer; }
  const chrono::ClockTimer &update_timer() const { return m_update_timer; }

  void set_duration(const chrono::MicroTime &duration) {
    m_duration = duration;
  }

  void set_minimum_duration(const chrono::MicroTime &duration) {
    m_minimum_duration = duration;
  }

  void set_update_period(const chrono::MicroTime &update_period) {
    m_update_period = update_period;
  }

  const chrono::MicroTime &duration() const { return m_duration; }
  const chrono::MicroTime &update_period() const { return m_update_period; }

private:
  var::Queue<var::String> m_queue;
  thread::Mutex m_queue_mutex;
  chrono::MicroTime m_minimum_duration;
  chrono::MicroTime m_duration;
  chrono::MicroTime m_update_period;
  chrono::ClockTimer m_duration_timer;
  chrono::ClockTimer m_update_timer;
  bool m_is_initialized;
};

#endif // UPDATER_HPP
