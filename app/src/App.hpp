#ifndef APP_HPP
#define APP_HPP

#include <cloud.hpp>
#include <json/Json.hpp>
#include <service.hpp>
#include <sys/Cli.hpp>
#include <var/String.hpp>
#include <var/Vector.hpp>
#include <sys/Process.hpp>

#include "SlPrinter.hpp"
#include "settings/Credentials.hpp"
#include "settings/SessionSettings.hpp"
#include "settings/WorkspaceSettings.hpp"
#include "utilities/OperatingSystem.hpp"

//#define VERSION __PROJECT

struct AppMembers {
private:
  SlPrinter printer;
  cloud::CloudService cloud_service;
  Credentials credentials;

  WorkspaceSettings workspace_settings;
  SessionSettings session_settings;
  GlobalSettings global_settings;


  static AppMembers & instance(){
    static AppMembers m_app_members;
    return m_app_members;
  }

  friend class AppAccess;
  AppMembers();
};

class AppAccess : public api::ExecutionContext {
public:
  enum class IsBootloaderOk { no, yes };
  enum class IsSuppressError { no, yes };
  enum class IsForceReinstall { no, yes };
  enum class IsForceDownloadSettings { no, yes };

  static SlPrinter &printer() { return AppMembers::instance().printer; }
  static cloud::CloudService &cloud_service() { return AppMembers::instance().cloud_service; }
  static bool is_cloud_service_available();
  static Credentials &credentials() { return AppMembers::instance().credentials; }

  static WorkspaceSettings &workspace_settings() {
    return AppMembers::instance().workspace_settings;
  }
  static SessionSettings &session_settings() {
    return AppMembers::instance().session_settings;
  }
  static GlobalSettings &global_settings() { return AppMembers::instance().global_settings; }

  static void save_credentials() { credentials().save(); }

  static constexpr int user_error_code() { return 200; }

  class ExecuteSystemCommandOptions {
    API_ACCESS_COMPOUND(ExecuteSystemCommandOptions, var::StringView, command);
    API_ACCESS_COMPOUND(
      ExecuteSystemCommandOptions,
      var::StringView,
      working_directory);
    API_ACCESS_BOOL(ExecuteSystemCommandOptions, silent, false);
  };

  static int execute_system_command(const sys::Process::Arguments & arguments, const var::StringView working_directory);

  static bool is_cloud_ready(
    IsForceDownloadSettings is_force_download_settings
    = IsForceDownloadSettings::no,
    IsSuppressError is_suppress_error = IsSuppressError::no);

  static bool is_admin() {
#if defined __admin_release
    return true;
#else
    return false;
#endif
  }

private:
};

class App : public AppAccess {
public:
  static int initialize(const sys::Cli &cli);
  static int initialize_cloud(
    IsForceDownloadSettings is_force_download_settings
    = IsForceDownloadSettings::no);
  static int finalize();
  static void launch_browser_login();

  static bool execute_update(const StringView version);
  static bool create_sl_copy(const StringView destination);

private:
  enum class IsPreview {
    no, yes
  };

  static int check_for_update(IsPreview is_preview);
  static void load_credentials();
  static bool download_sl_image(const StringView version, IsPreview is_preview);
  static var::StringView sl_admin_name() { return "sla"; }
  static var::StringView sl_update_name() { return "slu"; }
  static var::StringView sl_copy_name() { return "slc"; }
};

#define APP_RETURN_ASSIGN_ERROR(ERROR_MESSAGE)                                 \
  API_RETURN_VALUE_ASSIGN_ERROR(                                               \
    false,                                                                     \
    StringView(ERROR_MESSAGE).to_string().cstring(),                           \
    user_error_code())

#define APP_CALL_GRAPH_TRACE_FUNCTION()

#define APP_CALL_GRAPH_TRACE_CLASS_FUNCTION(name_value)
#define APP_CALL_GRAPH_TRACE_GROUP_FUNCTION()
#define APP_CALL_GRAPH_TRACE_EXTERNAL(function)

#define APP_CALL_GRAPH_TRACE_MESSAGE(message)

#endif // APP_HPP
