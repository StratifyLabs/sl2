// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef SDK_HPP
#define SDK_HPP

#include "Connector.hpp"

class Sdk : public Connector {
public:
  Sdk();

private:
  var::Vector<SdkProject> m_project_list;

  var::StringViewList get_command_list() const override;
  bool execute_command_at(u32 list_offset, const Command &command) override;

  bool install(const Command &command);
  bool update(const Command &command);
  bool publish(const Command &command);
  bool copyright(const Command &command);
  bool export_command(const Command &command);

  enum commands {
    command_install,
    command_update,
    command_publish,
    command_copyright,
    command_export,
    command_total
  };

  bool download_and_execute(
    const var::String &url,
    const var::String &command,
    const var::String &archive_name);
  bool get_directory_entries(
    var::Vector<var::String> &list,
    const var::String &path);
  var::Vector<SdkProject>
  bootstrap_sdk_workspace(const var::String &make_options);

  class AndroidOptions {
    API_ACCESS_COMPOUND(AndroidOptions, var::String, name);
    API_ACCESS_COMPOUND(AndroidOptions, var::String, destination);
    API_ACCESS_COMPOUND(AndroidOptions, var::String, source);
  };

  bool export_android_makefiles(const AndroidOptions &options) const;
};

#endif // SDK_HPP
