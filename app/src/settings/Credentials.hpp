#ifndef CONTAINERS_CREDENTIALS_HPP
#define CONTAINERS_CREDENTIALS_HPP

#include <fs/FileSystem.hpp>
#include <json/Json.hpp>
#include <json/JsonDocument.hpp>
#include <var/String.hpp>

#include "FilePathSettings.hpp"

class Credentials : public json::JsonValue {
public:
  Credentials() {}

  Credentials(const json::JsonObject &object) : json::JsonValue(object) {}
  JSON_ACCESS_STRING(Credentials, uid);
  JSON_ACCESS_STRING(Credentials, token);
  JSON_ACCESS_STRING_WITH_KEY(Credentials, refreshToken, refresh_token);
  JSON_ACCESS_INTEGER_WITH_KEY(Credentials, tokenTimestamp, token_timestamp);
  JSON_ACCESS_STRING_WITH_KEY(Credentials, sessionTicket, session_ticket);
  JSON_ACCESS_BOOL(Credentials, global);

  var::PathString path() const {
    return is_global() ? FilePathSettings::global_credentials_path()
                       : var::PathString(FilePathSettings::credentials_path());
  }

  Credentials &remove() {
    fs::FileSystem().exists(path()) && fs::FileSystem().remove(path()).is_success();
    *this = Credentials();
    return *this;
  }

  const Credentials &save() const {
    json::JsonDocument().save(
      to_object(),
      fs::File(fs::File::IsOverwrite::yes, path()));
    return *this;
  }

  Credentials &load() {
    API_RETURN_VALUE_IF_ERROR(*this);
    api::ErrorGuard error_guard;
    *this = json::JsonDocument()
              .load(fs::File(FilePathSettings::credentials_path()))
              .to_object();

    if (is_error()) {
        API_RESET_ERROR();
        api::ErrorGuard error_guard;
      to_object() = json::JsonDocument().load(
        fs::File(FilePathSettings::global_credentials_path()));
      set_global(true);
    } else {
      set_global(false);
    }
    return *this;
  }

private:
};

#endif // CREDENTIALS_HPP
