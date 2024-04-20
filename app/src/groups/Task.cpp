// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved

#include <chrono.hpp>
#include <fs.hpp>
#include <printer.hpp>
#include <sos.hpp>
#include <var.hpp>

#include "Task.hpp"

sos::TaskManager::Info TaskSnapshot::m_invalid_info
  = sos::TaskManager::Info::invalid();
TaskSnapshot Task::m_latest_snapshot(chrono::ClockTime::get_system_time());

Task::Task() : Updater("task", "task") {
  m_max_queue_count = 5000;
  set_update_period(100_milliseconds);
}

Task::~Task() {}

bool Task::initialize() {

  m_task_manager = TaskManager("", connection()->driver());
  start_timers();
  set_initialized(false);
  m_start_time = chrono::ClockTime::get_system_time();
  SL_PRINTER_TRACE("opened task manager");
  return true;
}

bool Task::update() {

  if (is_time_to_update()) {
    // load information for all tasks
    const chrono::ClockTime timestamp = m_start_time.get_age();
    m_timer.start();

    TaskSnapshot snapshot(timestamp);
    TaskSnapshot update_latest_snapshot(timestamp);
    auto task_info_list = m_task_manager.get_info();

    for (auto &info : task_info_list) {
      if (info.is_valid() && info.is_enabled()) {
        if (m_filter_list.is_task_filtered(info) == false) {
          info.set_name(m_filter_list.get_task_name(info)
                          .string_view()); // check for thread stack names
          snapshot.task_info_list().push_back(info);
        }
        update_latest_snapshot.task_info_list().push_back(info);
      }
    }

    m_latest_snapshot = update_latest_snapshot;

    // history is a queue of snapshots
    m_task_history.push_back(m_latest_snapshot);
    if (m_task_history.count() == m_max_queue_count) {
      m_task_history.pop_front();
    }
  }

  return is_running();
}

TaskSnapshotSummary &
TaskSnapshotSummary::operator<<(const sos::TaskManager::Info &info) {

  if (info.is_valid()) {
    if (last_info().is_valid() && info.timer() > last_info().timer()) {
      m_cpu_time += info.timer() - last_info().timer();
    }

    if (info.stack_size() > maximum_stack_size()) {
      set_maximum_stack_size(info.stack_size());
    }

    if (info.heap_size() > maximum_heap_size()) {
      set_maximum_heap_size(info.heap_size());
    }
  }
  set_last_info(info);

  return *this;
}


#if 0
var::String
Task::create_analysis_chart(const TaskSnapshotList &snapshot_list) const {


	ChartJs chart;
	chart.set_type(ChartJs::type_line);

	u32 color = 0;
	for (const TaskSummaryList &list : task_info_table.table()) {

		ChartJsDataSet memory_usage_data_set
				= ChartJsDataSet()
				.set_label(
					"memory " + list.name()
					+ String().format(" %d.%d", list.pid(), list.thread_id()))
				.set_background_color(ChartJsColor::create_transparent())
				.set_border_color(ChartJsColor::get_standard(color++).set_alpha(128));

		ChartJsDataSet cpu_usage_data_set
				= ChartJsDataSet()
				.set_label(
					"CPU " + list.name()
					+ String().format(" %d.%d", list.pid(), list.thread_id()))
				.set_background_color(ChartJsColor::create_transparent())
				.set_border_color(ChartJsColor::get_standard(color++).set_alpha(128));

		for (const TaskSummary &summary : list.list()) {
			String timestamp = summary.timestamp().to_string();
			// printer().object(timestamp, task_info_snapshot.task_info());

			memory_usage_data_set.append(
						ChartJsStringDataPoint()
						.set_x(timestamp)
						.set_y(NumberString(summary.memory_usage()))
						.to_object());

			cpu_usage_data_set.append(
						ChartJsStringDataPoint()
						.set_x(timestamp)
						.set_y(NumberString(
										 summary.cpu_time() * 100.0f / summary.total_cpu_time()))
						.to_object());
		}

		chart.data().append(memory_usage_data_set).append(cpu_usage_data_set);
	}

	chart.options()
			.set_scales(
				ChartJsScales()
				.append_x_axis(
					ChartJsAxis()
					.set_display(true)
					.set_type(ChartJsAxis::type_linear)
					.set_scale_label(
						ChartJsScaleLabel().set_display(true).set_label("timestamp")))
				.append_y_axis(
					ChartJsAxis()
					.set_type(ChartJsAxis::type_linear)
					.set_display(true)
					.set_ticks(
						ChartJsAxisTicks().set_minimum(0).set_maximum(100).set_step_size(
							10))
					.set_scale_label(ChartJsScaleLabel().set_display(true).set_label(
														 "Utilization Percentage"))))
			.set_legend(ChartJsLegend().set_display(true))
			.set_title(ChartJsTitle().set_display(true).set_text("Task Analysis"));

	return JsonDocument().stringify(chart.to_object());
}

#endif

bool Task::finalize() {


  m_task_manager = TaskManager();

  if (is_initialized() == false) {
    return true;
  }

  m_timer.stop();
  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();
  SlPrinter::Output printer_output_guard(printer());

  SL_PRINTER_TRACE(
    String().format("history has %d items", m_task_history.count()));

  TaskSnapshotList snapshot_list(m_task_history);
  SL_PRINTER_TRACE("Get unique task list");

  if (snapshot_list.unique_task_list().count()) {

    SL_PRINTER_TRACE("Get total CPU time");

    {
      SL_PRINTER_TRACE("timing object");
      json::JsonObject timing_object;
      timing_object.insert(
        "cpuCycles",
        JsonString(
          String().format("%ld cycles", snapshot_list.total_cpu_time())));
      timing_object.insert(
        "totalTime",
        JsonString(String().format("%ld us", m_timer.microseconds())));

      float cpuFrequency = 1.0f * snapshot_list.total_cpu_time()
                           / (1.0f * m_timer.microseconds() / 1000000.0f);
      timing_object.insert(
        "cpuFrequency",
        JsonString(String().format(
          "~%0.3f MHz",
          static_cast<double>(cpuFrequency / 1000000.0f))));

      printer().object("timing", timing_object);
    }

    {
      // summary table

      printer().insert_newline();

      printer().start_table(StringViewList(
        {"process",
         "name",
         "id",
         "pid",
         "cpuTime",
         "cpuUsage",
         "maximumStack",
         "maximumHeap",
         "maximumMemoryUsage"}));

      for (const sos::TaskManager::Info &task :
           snapshot_list.unique_task_list()) {
        TaskSnapshotSummary summary = snapshot_list.get_task_snapshot_summary(
          TaskSnapshotList::GetSnapshotSummaryOptions().set_task_info(task));

        printer().append_table_row(StringViewList(
          {String().format(
             "%s-%d.%d",
             NameString(summary.task_info().name()).cstring(),
             summary.task_info().id(),
             summary.task_info().pid()),
           summary.task_info().name(),
           String().format(F32U, summary.task_info().id()),
           String().format(F32U, summary.task_info().pid()),
           String().format("%lld", summary.cpu_time()),
           String().format(
             "%0.6f",
             static_cast<double>(
               summary.get_cpu_utilization(snapshot_list.total_cpu_time()))),
           String().format("%ld", summary.maximum_stack_size()),
           summary.task_info().is_thread()
             ? String("NA")
             : String().format("%ld", summary.maximum_heap_size()),
           String().format(
             "%0.2f",
             static_cast<double>(summary.get_memory_utilization()))}));
      }
      printer().finish_table(printer::Printer::Level::info);
    }

  }
  SL_PRINTER_TRACE("finalize task manager");
  return is_success();
}

var::StringViewList Task::get_command_list() const {

  StringViewList list = {"list", "signal", "analyze"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool Task::execute_command_at(u32 list_offset, const Command &command) {


  switch (list_offset) {
  case command_list:
    return list(command);
  case command_signal:
    return signal(command);
  case command_analyze:
    return analyze(command);
  }

  return false;
}

bool Task::list(const Command &command) {


  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      list,
      "lists the currently active tasks (threads and processes).")
      + GROUP_ARG_OPT(
        id_tid,
        int,
        <all>,
        "task ID to filter (will show a max of one entry).")
      + GROUP_ARG_OPT(
        name,
        string,
        <all>,
        "application name to filter tasks (or use `pid`). Use `?` to separate "
        "list items")
      + GROUP_ARG_OPT(
        pid,
        int,
        <all>,
        "process id used to filter tasks (or use `name`)."));

  if (!command.is_valid(reference, printer())) {

    return printer().close_fail();
  }

  const StringView pid = command.get_argument_value("pid");
  const StringView name = command.get_argument_value("name");
  const StringView id = command.get_argument_value("id"); // id or task ID

  command.print_options(printer());

  if (is_connection_ready() == false) {
    return printer().close_fail();
  }
  SlPrinter::Output printer_output_guard(printer());

  // print out the tasks
  sos::TaskManager task_manager("", connection()->driver());

  FilterList filter_list = FilterList(name);

  printer().start_table(
    {"process",
     "name",
     "id",
     "pid",
     "priority",
     "memorySize",
     "stack",
     "stackSize",
     "isThread",
     "heap",
     "heapSize"});

  int task_id = 0;
  sos::TaskManager::Info info = task_manager.get_info(task_id++);
  while (is_success()) {
    if (info.is_enabled()) {
      bool is_filtered = false;
      if (!pid.is_empty() && (info.pid() != pid.to_unsigned_long())) {
        is_filtered = true;
      }

      if (filter_list.is_task_filtered(info)) {
        is_filtered = true;
      }

      if (!id.is_empty() && (info.id() != id.to_unsigned_long())) {
        is_filtered = true;
      }

      if (!is_filtered) {

        if (info.pid() == 0 && info.thread_id() == 0) {
          info.set_name("idle");
        }

        info.set_name(filter_list.get_task_name(info));

        push_queue(JsonDocument().stringify(
          JsonObject()
            .insert("name", JsonString(info.name()))
            .insert("id", JsonInteger(info.id()))
            .insert("pid", JsonInteger(info.pid()))
            .insert("priority", JsonInteger(info.priority()))
            .insert("memorySize", JsonInteger(info.memory_size()))
            .insert("memoryUtilization", JsonInteger(info.memory_utilization()))
            .insert("stack", JsonInteger(info.stack()))
            .insert("stackSize", JsonInteger(info.stack_size()))
            .insert_bool("isThread", info.is_thread())
            .insert("heap", JsonInteger(info.heap()))
            .insert("heapSize", JsonInteger(info.heap_size()))));

        APP_CALL_GRAPH_TRACE_MESSAGE(info.name());
        printer().append_table_row(
          {String().format(
             "%s-%d.%d",
             NameString(info.name()).cstring(),
             info.id(),
             info.pid()),
           info.name(),
           String().format("%d", info.id()),
           String().format("%d", info.pid()),
           String()
             .format(F32U ":" F32U, info.priority(), info.priority_ceiling()),
           String().format(F32U, info.memory_size()),
           String().format("0x%lX", info.stack()),
           String().format(F32U, info.stack_size()),
           info.is_thread() ? "true" : "false",
           info.is_thread() ? String("NA")
                            : String().format("0x%lX", info.heap()),
           info.is_thread() ? String("NA")
                            : String().format("%ld", info.heap_size())});
      }
    }
    info = task_manager.get_info(task_id++);
  }
  API_RESET_ERROR();

  printer().finish_table(printer::Printer::Level::info);

  // This is available in all editors.
  return is_success();
}

bool Task::signal(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      signal,
      "sends a POSIX style signal to the process running on the connected "
      "device.")
      + GROUP_ARG_OPT(
        id_tid,
        int,
        <any>,
        "thread ID to receive the signal. If specified, pthread_kill() is used "
        "instead of kill().")
      + GROUP_ARG_OPT(
        name_n,
        string,
        <any>,
        "application name to receive the signal (or use 'pid').")
      + GROUP_ARG_OPT(
        pid,
        int,
        <any>,
        "process ID target for the signal (or use 'name'). If specified kill() "
        "is used rather than pthread_kill().")
      + GROUP_ARG_REQ(
        signal_s,
        int,
        <signal>,
        "signal to send which can be *KILL*, *INT*, *STOP*, *ALARM*, *TERM*, "
        "*BUS*, *QUIT*, and *CONTINUE*")
      + GROUP_ARG_OPT(
        value_v,
        int,
        <none>,
        "optional signal value that can be sent with the associated signal "
        "number"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView pid = command.get_argument_value("pid");
  StringView name = command.get_argument_value("name");
  StringView id = command.get_argument_value("id");
  StringView sig = command.get_argument_value("signal");
  StringView value = command.get_argument_value("value");

  int signo = -1;
  int pid_value = -1;

  const NameString sig_upper = NameString(sig).to_upper();
  if (sig_upper == "KILL") {
    signo = LINK_SIGKILL;
  }
  if (sig_upper == "INT") {
    signo = LINK_SIGINT;
  }
  if (sig_upper == "TERM") {
    signo = LINK_SIGTERM;
  }
  if (sig_upper == "STOP") {
    signo = LINK_SIGSTOP;
  }
  if (sig_upper == "ALRM") {
    signo = LINK_SIGALRM;
  }
  if (sig_upper == "ALARM") {
    signo = LINK_SIGALRM;
  }
  if (sig_upper == "BUS") {
    signo = LINK_SIGBUS;
  }
  if (sig_upper == "QUIT") {
    signo = LINK_SIGQUIT;
  }
  if (sig_upper == "CONT") {
    signo = LINK_SIGCONT;
  }
  if (sig_upper == "CONTINUE") {
    signo = LINK_SIGCONT;
  }

  // now get the task info for the PID
  command.print_options(printer());

  if (is_connection_ready() == false) {
    return is_success();
  }

  sos::TaskManager task_manager("", connection()->driver());

  SlPrinter::Output printer_output_guard(printer());
  Printer::Object printer_object(printer().active_printer(), sig);

  if (signo < 0) {
    printer().error("Signal not recognized " + sig);
    printer().close_fail();
  }

  if (name.is_empty() == false) {
    pid_value = task_manager.get_pid(name);
    if (pid_value <= 0) {
      APP_RETURN_ASSIGN_ERROR("failed to find pid for name " + name);
    }

  } else if (pid.is_empty() == false) {
    SL_PRINTER_TRACE("Get pid from argument " + pid);
    pid_value = pid.to_integer();
  } else if (id.is_empty()) {
    APP_RETURN_ASSIGN_ERROR("pid, name, or id must be specified");
  }

  sys_killattr_t kill_attr;
  int request;

  if (id.is_empty()) {
    request = I_SYS_KILL;
    printer().key("action", String("kill"));
    kill_attr.id = static_cast<u32>(pid_value);
  } else {
    printer().key("action", String("pthread kill"));
    request = I_SYS_PTHREADKILL;
    kill_attr.id = id.to_unsigned_long();
  }

  kill_attr.si_signo = static_cast<u32>(signo);
  kill_attr.si_sigvalue = value.to_integer();
  kill_attr.si_sigcode = LINK_SI_USER;

  Link::File device_system(
    "/dev/sys",
    OpenMode::read_write(),
    connection()->driver());

  printer().key("signal", sig + "(" + NumberString(signo) + ")");
  printer().key("id", String().format(F32U, kill_attr.id).string_view());
  device_system.ioctl(request, &kill_attr);

  SL_PRINTER_TRACE(
    GeneralString("is success ") | (is_success() ? "true" : "false"));

  return is_success();
}

bool Task::analyze(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      analyze,
      "samples the state of the threads/processes running in the system and "
      "print a report when the applciation "
      "exits.")
      + GROUP_ARG_OPT(
        duration_d,
        string,
        <indefinite>,
        "number of seconds to monitor (default is while the terminal is "
        "running).")
      + GROUP_ARG_OPT(
        name_n,
        string,
        <all>,
        "application name (or path to the project) to monitor (default is to "
        "monitor all "
        "threads/processes). Use `?` to do multiple names.")
      + GROUP_ARG_OPT(
        period_p,
        int,
        100,
        "period in milliseconds to sample task activity."));

  if (!command.is_valid(reference, printer())) {

    return printer().close_fail();
  }

  StringView name = command.get_argument_value("name");
  StringView duration = command.get_argument_value("duration");
  StringView period = command.get_argument_value("period");
  StringView is_save = command.get_argument_value("save");
  StringView is_details = command.get_argument_value("details");

  if (is_save.is_empty()) {
    is_save = "false";
  }
  if (is_details.is_empty()) {
    is_details = "false";
  }
  if (period.is_empty() || (period.to_integer() == 0)) {
    period = "100";
  }

  command.print_options(printer());

  if (is_connection_ready() == false) {
    return printer().close_fail();
  }
  SlPrinter::Output printer_output_guard(printer());

  m_filter_list = FilterList(name);

  printer().info("task analysis enabled");
  printer().debug("preparing for task analysis");
  set_update_period(period.to_integer() * 1_milliseconds);
  set_duration(duration.to_integer() * 1_seconds);
  start_timers();

  return is_success();
}
