// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef TASK_HPP
#define TASK_HPP

#include <fs.hpp>
#include <service.hpp>
#include <sos.hpp>
#include <var.hpp>

#include "utilities/Updater.hpp"

class TaskSnapshot {
public:
  TaskSnapshot(const chrono::ClockTime &timestamp) : m_timestamp(timestamp) {}

  const chrono::ClockTime &timestamp() const { return m_timestamp; }

  class QueryOptions {
    API_AC(QueryOptions, var::String, name);
    API_AF(QueryOptions, int, thread_id, -1);
    API_AF(QueryOptions, int, pid, -1);
  };

  const sos::TaskManager::Info &task_info(const sos::TaskManager::Info &task) const {
    for (const sos::TaskManager::Info &info : task_info_list()) {
      if (
        (task.name().is_empty() || task.name() == info.name())
        && (static_cast<int>(task.pid()) < 0 || task.pid() == info.pid())
        && (static_cast<int>(task.thread_id()) < 0 || task.thread_id() == info.thread_id())) {
        return info;
      }
    }
    return m_invalid_info;
  }

  u64 get_minimum_cpu_time() const {
    u64 result = 0;
    for (const auto &info : task_info_list()) {
      if (result == 0 || info.timer() < result) {
        result = info.timer();
      }
    }
    return result;
  }

  u64 get_maximum_cpu_time() const {
    u64 result = 0;
    for (const auto &info : task_info_list()) {
      if (result == 0 || info.timer() > result) {
        result = info.timer();
      }
    }
    return result;
  }

  static u64
  get_total_cpu_time(const TaskSnapshot &first, const TaskSnapshot &last) {
    u64 result = 0;
    for (const sos::TaskManager::Info &info : first.task_info_list()) {
      u32 offset = last.task_info_list().find_offset(info);
      if (offset < last.task_info_list().count()) {
        if (last.task_info_list().at(offset).timer() > info.timer()) {
          result += (last.task_info_list().at(offset).timer() - info.timer());
        }
      }
    }
    return result;
  }

private:
  chrono::ClockTime m_timestamp;
  API_AC(TaskSnapshot, var::Vector<sos::TaskManager::Info>, task_info_list);
  static sos::TaskManager::Info m_invalid_info;
};

class TaskSnapshotSummary {
public:
  TaskSnapshotSummary() : m_task_info(sos::TaskManager::Info::invalid()) {}

  TaskSnapshotSummary(
    const sos::TaskManager::Info &info,
    const chrono::ClockTime &timestamp)
    : m_task_info(info), m_timestamp(timestamp) {
    m_maximum_stack_size = info.stack_size();
    m_maximum_heap_size = info.heap_size();
    set_last_info(sos::TaskManager::Info::invalid());
  }

  bool is_valid() const { return task_info().is_valid(); }

  TaskSnapshotSummary &operator<<(const sos::TaskManager::Info &info);

  bool operator==(const TaskSnapshotSummary &a) const {
    return (a.task_info().id() == task_info().id())
           && (a.task_info().pid() == task_info().pid());
  }

  float get_memory_utilization() const {
    float result = 0.0f;
    if (task_info().is_thread()) {
      result = (maximum_stack_size() * 100.0f) / task_info().memory_size();
    } else {
      result = ((maximum_heap_size() + maximum_stack_size()) * 100.0f)
               / task_info().memory_size();
    }
    return result;
  }

  float get_cpu_utilization(u64 total) const {
    return cpu_time() * 100.0f / total;
  }

private:
  API_AC(TaskSnapshotSummary, sos::TaskManager::Info, task_info);
  API_AC(TaskSnapshotSummary, sos::TaskManager::Info, last_info);
  API_AC(TaskSnapshotSummary, chrono::ClockTime, timestamp);
  API_AF(TaskSnapshotSummary, u64, cpu_time, 0);
  API_AF(TaskSnapshotSummary, u64, total_cpu_time, 0);
  API_AF(TaskSnapshotSummary, u32, maximum_heap_size, 0);
  API_AF(TaskSnapshotSummary, u32, maximum_stack_size, 0);
};

class TaskSnapshotList : public AppAccess {
public:
  TaskSnapshotList(const var::Deque<TaskSnapshot> &history) {
    list().reserve(history.count());
    for (const auto &item : history) {
      list().push_back(item);
    }

    for (const TaskSnapshot &snapshot : list()) {
      for (const sos::TaskManager::Info &task_info : snapshot.task_info_list()) {
        u32 offset = m_unique_task_list.find_offset(task_info);
        if (offset == m_unique_task_list.count()) {
          m_unique_task_list.push_back(task_info);
        }
      }
    }

    u64 sum_cpu_time = 0;
    for (const sos::TaskManager::Info &task : unique_task_list()) {
      TaskSnapshotSummary summary = get_task_snapshot_summary(
        TaskSnapshotList::GetSnapshotSummaryOptions().set_task_info(task));
      if (summary.is_valid()) {
        sum_cpu_time += summary.cpu_time();
      }
    }
    m_total_cpu_time = sum_cpu_time;
  }

  size_t count() const { return list().count(); }

  class GetSnapshotSummaryOptions {
    API_AF(GetSnapshotSummaryOptions, u32, offset, 0);
    API_AF(GetSnapshotSummaryOptions, u32, count, 0);
    API_AC(GetSnapshotSummaryOptions, sos::TaskManager::Info, task_info);
  };

  TaskSnapshotSummary
  get_task_snapshot_summary(const GetSnapshotSummaryOptions &options) const {
    API_ASSERT(options.count() != 1);
    u32 count = options.count();
    if (count == 0) {
      count = list().count();
    }
    if (options.offset() + count > list().count()) {
      SL_PRINTER_TRACE("offset invalid for " + options.task_info().name());
      return TaskSnapshotSummary();
    }

    u32 find_first = options.offset();
    const TaskSnapshot *first_pointer;
    {
      bool is_valid = false;
      do {
        first_pointer = &list().at(find_first);
        is_valid = first_pointer->task_info(options.task_info()).is_valid();
        if (!is_valid) {
          find_first++;
        }
      } while (!is_valid && find_first < options.offset() + count);

      if (find_first == options.offset() + count) {
        SL_PRINTER_TRACE("Didn't find first for " + options.task_info().name());
        return TaskSnapshotSummary();
      }
    }

    u32 find_last = options.offset() + count - 1;
    const TaskSnapshot *last_pointer;
    {
      bool is_valid = false;
      do {
        last_pointer = &list().at(find_last);
        is_valid = last_pointer->task_info(options.task_info()).is_valid();
        if (!is_valid) {
          find_last--;
        }
      } while (!is_valid && find_last > options.offset());

      if (find_last == options.offset() + count) {
        return TaskSnapshotSummary();
      }
    }

    const TaskSnapshot &first(*first_pointer);
    const TaskSnapshot &last(*last_pointer);

    // get total CPU time between first and last

    TaskSnapshotSummary result = TaskSnapshotSummary(
      first.task_info(options.task_info()),
      first.timestamp());

    result.set_total_cpu_time(TaskSnapshot::get_total_cpu_time(first, last));

    for (u32 i = 0; i < count; i++) {
      // this will set the stack and heap to the max over the window
      result << list().at(options.offset() + i).task_info(options.task_info());
    }

    return result;
  }

private:
  API_AC(TaskSnapshotList, var::Vector<TaskSnapshot>, list);
  API_RAF(TaskSnapshotList, u64, total_cpu_time, 0);
  API_RAC(TaskSnapshotList, var::Vector<sos::TaskManager::Info>, unique_task_list);
};

class Task : public Updater {
public:
  Task();
  ~Task();

  bool initialize();
  bool finalize();
  bool update() override;

  static int get_pid_from_name(const var::StringView name) {
    const sos::TaskManager::Info &info
      = m_latest_snapshot.task_info(sos::TaskManager::Info().set_name(name));
    if (info.is_valid()) {
      return info.pid();
    }
    return -1;
  }

  static bool is_pid_running(int pid) {
    if (pid == 0) {
      return true;
    }

    return m_latest_snapshot.task_info(sos::TaskManager::Info().set_pid(pid))
      .is_valid();
  }

  bool analyze(const Command &command);

private:
  class FilterList {

    class ThreadStackItem {
      API_AC(ThreadStackItem, var::String, name);
      API_AC(ThreadStackItem, Project::ThreadStackItem, stack_item);

    public:
      ThreadStackItem() : m_stack_item("") {}
    };

    API_AC(FilterList, var::StringList, name_list);
    API_AC(FilterList, var::Vector<ThreadStackItem>, thread_stack_list);

  public:
    FilterList() {}
    explicit FilterList(const var::StringView name) {

      var::StringViewList name_filter_list = name.split("?");
      if (name.is_empty() == false) {
        for (const StringView item : name_filter_list) {
          name_list().push_back(fs::Path::name(item).to_string());
          if (name_list().back() != "sys") {
            Project project = Project().import_file(
              File(FilePathSettings::project_settings_file_path(item)
                     .string_view()));
            if (project.is_valid()) {
              for (const auto &stack_item : project.get_thread_stack_list()) {
                thread_stack_list().push_back(
                  ThreadStackItem()
                    .set_stack_item(stack_item)
                    .set_name(fs::Path::name(item).to_string()));
              }
            }
          }
        }
      }
    }

    var::NameString get_task_name(const sos::TaskManager::Info &info) {

      if (info.pid() == 0 && info.thread_id() == 0) {
        return "idle";
      }

      for (const ThreadStackItem &item : thread_stack_list()) {
        if (
          info.is_thread() && info.name() == item.name().string_view()
          && info.memory_size() == item.stack_item().value().to_integer()) {
          return var::NameString(item.stack_item().key().string_view());
        }
      }
      return info.name();
    }

    bool is_task_filtered(const sos::TaskManager::Info &info) {
      return name_list().count()
             && (name_list().find_offset(info.name().to_string()) == name_list().count());
    }
  };

  u32 m_max_queue_count;
  sos::TaskManager m_task_manager;
  var::Deque<TaskSnapshot> m_task_history;
  chrono::ClockTimer m_timer;
  chrono::ClockTime m_start_time;
  static TaskSnapshot m_latest_snapshot;
  FilterList m_filter_list;

  enum commands {
    command_list,
    command_signal,
    command_analyze,
    command_total
  };

  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  // TaskSummaryTable create_task_summary_table(int total_samples = 50);
  // var::Vector<var::Datum> summarize_history(int samples_per_datum);

  bool list(const Command &command);
  bool signal(const Command &command);
};

#endif // TASK_HPP
