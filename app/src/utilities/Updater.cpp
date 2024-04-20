// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#include "Updater.hpp"

Updater::Updater(const char *name, const char *shortcut)
  : Connector(name, shortcut), m_update_period(100_milliseconds) {
  m_is_initialized = false;
}

bool Updater::is_time_to_update() {

  if (is_duration_complete()) {
    return false;
  }

  if (is_running() && (m_update_timer >= m_update_period)) {
    m_update_timer.restart();
    return true;
  }

  return false;
}

bool Updater::is_duration_complete() {

  if (is_running() == false) {
    return true;
  }

  // if duration is zero, go forever
  if (m_duration.seconds() == 0) {
    return false;
  }

  if (m_duration_timer >= m_duration) {
    stop_timers();
    return true;
  }

  return false;
}
