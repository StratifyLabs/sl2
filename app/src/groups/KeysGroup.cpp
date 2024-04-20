#include <crypto.hpp>
#include <service.hpp>
#include <var.hpp>

#include "KeysGroup.hpp"

KeysGroup::KeysGroup() : Group("keys", "key") {}

var::StringViewList KeysGroup::get_command_list() const {

  StringViewList list
    = {"ping", "publish", "sign", "verify", "revoke", "remove", "download"};
  API_ASSERT(list.count() == command_total);

  return list;
}

bool KeysGroup::execute_command_at(u32 list_offset, const Command &command) {

  switch (list_offset) {
  case command_ping:
    return ping(command);
  case command_publish:
    return publish(command);
  case command_sign:
    return sign(command);
  case command_verify:
    return verify(command);
  case command_revoke:
    return verify(command);
  case command_remove:
    return remove(command);
  case command_download:
    return download(command);
  }
  return false;
}

bool KeysGroup::ping(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(ping, "pings the information about the keys.")
      + GROUP_ARG_OPT(hardware_id, string, <current>, "id to ping.")
      + GROUP_ARG_OPT(
        password_pwd,
        string,
        <none>,
        "password used to decrypt the private key, if not provided encrypted "
        "private key is shown.")
      + GROUP_ARG_OPT(
        code_c,
        bool,
        false,
        "format the key output in a way that can be copied into C source "
        "code"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView identifier = command.get_argument_value("hardware");
  StringView password = command.get_argument_value("password");
  StringView code = command.get_argument_value("code");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  if (identifier.is_empty()) {
    identifier = cloud_service().cloud().credentials().get_uid();
  }

  SlPrinter::Output printer_output_guard(printer());

  if (password.is_empty() == false && password.length() != 64) {
    APP_RETURN_ASSIGN_ERROR(
      "password must have 64 characters if it is provided at all");
  }

  Keys keys_document(identifier);

  if (keys_document.is_valid() == false) {
    APP_RETURN_ASSIGN_ERROR(
      "the keys for id `" | identifier | "` do not exist");
  }

  if (is_error()) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to download user `" + identifier + "`");
  }

  printer().object(identifier, keys_document.to_object());

  auto key_to_code = [](const StringView key) -> GeneralString {
    GeneralString result;
    Array<u8, 256> buffer;
    API_ASSERT(key.length() <= 512);
    View(buffer).from_string(key);
    for (u32 i = 0; i < key.length() / 2; i++) {
      result |= NumberString(buffer.at(i), "0x%02x,");
    }
    return result.pop_back();
  };

  if (password.is_empty() == false) {
    const auto dsa = keys_document.get_digital_signature_algorithm(Aes::Key(
      Aes::Key::Construct().set_key(password).set_initialization_vector(
        keys_document.get_iv())));

    printer().output().object("digitalSignatureAlgorithm", dsa.key_pair());

    if (code == "true") {
      printer::Printer::Object code_object(printer().output(), "code");
      printer()
        .output()
        .key("publicKey", key_to_code(dsa.key_pair().public_key().to_string()))
        .key(
          "privateKey",
          key_to_code(dsa.key_pair().public_key().to_string()));
    }

  } else if (code == "true") {
    printer::Printer::Object code_object(printer().output(), "code");
    printer().output().key(
      "publicKey",
      key_to_code(keys_document.get_public_key()));
  }

  return is_success();
}

bool KeysGroup::publish(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      publish,
      "publish a pair of private/public keys used for code signing.")
      + GROUP_ARG_REQ(
        name,
        string,
        <name of the keys>,
        "the keys name is used to help identify what the keys are for.")
      + GROUP_ARG_OPT(
        secure,
        bool,
        true,
        "encrypt the private key using the password provided or a randomly "
        "generated password.")
      + GROUP_ARG_OPT(
        password_pwd,
        string,
        <random>,
        "password used to encrypt the private key.")
      + GROUP_ARG_OPT(team, string, <none>, "team to associate with this key.")
      + GROUP_ARG_OPT(
        permissions,
        string,
        public,
        "public|private|searchable permissions for the key (private key is "
        "always encrypted).")
      + GROUP_ARG_OPT(
        privatekey,
        string,
        <generated from random data>,
        "private key to use.")
      + GROUP_ARG_OPT(
        publickey,
        string,
        <generated from random data>,
        "public key to use."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView name = command.get_argument_value("name");
  const StringView secure = command.get_argument_value("secure");
  const StringView password = command.get_argument_value("password");
  const StringView team = command.get_argument_value("team");
  const StringView permissions = [&]() {
    const auto perms = command.get_argument_value("permissions");
    if (perms.is_empty()) {
      return StringView("public");
    }
    return perms;
  }();

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  if (Document::is_permissions_valid(permissions) == false) {
    APP_RETURN_ASSIGN_ERROR(
      "valid permissions are " | Document::get_valid_permissions());
  }

  Aes::Key aes_key;
  DigitalSignatureAlgorithm dsa(DigitalSignatureAlgorithm::Curve::secp256r1);

  if (!password.is_empty()) {
    // use user key
    if (password.length() != 64) {
      APP_RETURN_ASSIGN_ERROR(
        "Password must be 64 bytes in length representing a 256-bit (32 byte) "
        "AES key");
    }

    Aes::Key256 key_from_password;
    View(key_from_password).from_string(password);
    aes_key.set_key(key_from_password);
  }

  if (secure != "true") {
    aes_key.nullify();
  }

  const auto keys_document = [&]() {
    printer::Printer::Object po(printer().output(), "keys");
    printer()
      .output()
      .key("publicKey", dsa.key_pair().public_key().to_string())
      .key("privateKey (plain)", dsa.key_pair().private_key().to_string());

    auto keys_document = Keys(dsa, aes_key)
                           .set_name(name)
                           .set_team_id(team)
                           .set_permissions(permissions)
                           .save();

    const auto key_256 = keys_document.get_key256(aes_key);

    printer()
      .output()
      .key("secureKey256 (plain)", View(key_256).to_string<GeneralString>())
      .key("password", aes_key.get_key256_string());
    if (secure == "true") {
      printer().info(
        "you MUST save this password in order to use the `privateKey` and/or "
        "`secureKey256`");
    }
    return keys_document;
  }();

  if (is_error()) {
    APP_RETURN_ASSIGN_ERROR("failed to create new keys");
  }

  printer().object("published", keys_document.to_object());

  return is_success();
}

bool KeysGroup::sign(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(sign, "uses the specified key to sign a file or build.")
      + GROUP_ARG_OPT(key_id, string, <current>, "id of the key.")
      + GROUP_ARG_REQ(
        path,
        string,
        <path to file>,
        "path to the file that you want to sign.")
      + GROUP_ARG_OPT(
        append,
        bool,
        false,
        "creates a signed copy of the file specified by `path`.")
      + GROUP_ARG_OPT(
        suffix_extension,
        string,
        .signed,
        "the suffix to append when using `append`.")
      + GROUP_ARG_OPT(
        password_pwd,
        string,
        <null>,
        "password for the private key."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const StringView path = command.get_argument_value("path");
  const StringView identifier = command.get_argument_value("key");
  const auto password = [](StringView pass) {
    if (pass.is_empty()) {
      return String("0") * 64;
    }
    return String(pass);
  }(command.get_argument_value("password"));
  const StringView append = command.get_argument_value("append");
  const StringView suffix = command.get_argument_value("suffix");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  if (append == "true" && path.is_empty()) {
    APP_RETURN_ASSIGN_ERROR("`path` must be specified when using `append`");
  }

  if (password.length() != 64) {
    APP_RETURN_ASSIGN_ERROR(
      "password length must be 64 characters to have a 256-bit AES key");
  }

  if (
    FileSystem().exists(path) == false
    || FileSystem().get_info(path).is_file() == false) {
    APP_RETURN_ASSIGN_ERROR("could not find a file at " | path);
  }

  Keys keys_document(identifier);

  if (keys_document.is_valid() == false) {
    APP_RETURN_ASSIGN_ERROR(
      "the keys for id `" | identifier | "` do not exist");
  }

  Aes::Key aes_key(
    Aes::Key::Construct().set_key(password).set_initialization_vector(
      keys_document.get_iv()));

  if (aes_key.is_key_null() && aes_key.is_iv_null() == false) {
    APP_RETURN_ASSIGN_ERROR("the key id requires a non-null password");
  }

  auto dsa = keys_document.get_digital_signature_algorithm(aes_key);

  // hash the file

  const auto hash_string = [](const StringView path) {
    Sha256 hash;
    NullFile().write(File(path), hash);
    return hash.to_string();
  }(path);

  Sha256::Hash hash_value = Sha256::from_string(hash_string);

  {
    auto signature = dsa.sign(hash_value);
    const bool is_verified
      = keys_document.get_digital_signature_algorithm(Aes::Key().nullify())
          .verify(signature, hash_value);

    printer::Printer::Object po(printer().output(), "signature");
    printer()
      .output()
      .key("publicKey", dsa.key_pair().public_key().to_string())
      .key("sha256", hash_string)
      .key("signature", signature.to_string())
      .key_bool("verified", is_verified);

    if (is_verified == false) {
      APP_RETURN_ASSIGN_ERROR("failed to sign " | path);
    }

    if (append == "true") {
      const auto output_file_path = path & suffix;
      File output_file = File(File::IsOverwrite::yes, output_file_path)
                           .write(File(path))
                           .move();
      sos::Auth::append(output_file, signature);
      if (
        sos::Auth::verify(File(output_file_path), dsa.key_pair().public_key())
        == false) {
        APP_RETURN_ASSIGN_ERROR(
          "failed to verify signature on signed file: " | output_file_path);
      } else {
        printer().output().key("created", output_file_path);
      }
    }
  }

  if (is_error()) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to download user `" + identifier + "`");
  }

  return is_success();
}

bool KeysGroup::verify(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(
      verify,
      "uses the specified public key to verify a digital signature.")
      + GROUP_ARG_REQ(key_id, string, <current>, "id of the key.")
      + GROUP_ARG_OPT(
        signature_sig,
        string,
        <signature of hash>,
        "the digital signature to verify against the hash (must be specified "
        "if not available in the file).")
      + GROUP_ARG_OPT(
        path_source,
        string,
        <path to file>,
        "path to the file that you want to sign (must specify `path` or "
        "`hash`).")
      + GROUP_ARG_OPT(
        strip,
        string,
        <signed>,
        "If the signature is appended with this suffix, a new file will be "
        "created with the signature omitted.")
      + GROUP_ARG_OPT(
        hash,
        string,
        <hash to verify>,
        "256-bit hash (64 characters) to verify the signature on (must specify "
        "`path` or `hash`)."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto path = command.get_argument_value("path");
  const auto hash = command.get_argument_value("hash");
  const auto signature = command.get_argument_value("signature");
  const auto identifier = command.get_argument_value("key");
  const auto strip = command.get_argument_value("strip");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  if (path.is_empty() && hash.length() != 64) {
    APP_RETURN_ASSIGN_ERROR(
      "hash length must be 64 characters to have a 256-bit AES key");
  }

  if (path.is_empty() && hash.is_empty()) {
    APP_RETURN_ASSIGN_ERROR("you must specify a `path` or a `hash` to verify");
  }

  if (
    path.is_empty() == false
    && (FileSystem().exists(path) == false || FileSystem().get_info(path).is_file() == false)) {
    APP_RETURN_ASSIGN_ERROR("could not find a file at " | path);
  }

  SlPrinter::Object so(printer().output(), "verify");
  const auto signature_info = sos::Auth::get_signature_info(File(path));

  const auto is_signed
    = !path.is_empty() && signature_info.signature().is_valid();
  if (signature.is_empty() && is_signed == false) {
    APP_RETURN_ASSIGN_ERROR(
      "please specify a signed file or a `signature` " | path
      | " is not signed");
  }

  Keys keys_document(identifier);

  const auto effective_signature
    = is_signed ? signature_info.signature() : Dsa::Signature(signature);

  if (keys_document.is_valid() == false) {
    APP_RETURN_ASSIGN_ERROR(
      "the keys for id `" | identifier | "` do not exist");
  }

  auto dsa
    = keys_document.get_digital_signature_algorithm(Aes::Key().nullify());

  const auto is_verified = [&]() {
    if (is_signed) {
      printer().key(
        "hash",
        View(signature_info.hash()).to_string<GeneralString>());
      return sos::Auth::verify(File(path), dsa.key_pair().public_key());
    } else {
      // hash the file
      const auto hash_string
        = [](const StringView path, const StringView hash) {
            if (path.is_empty() == false) {
              Sha256 sha256;
              NullFile().write(File(path), sha256);
              return sha256.to_string();
            }
            return var::GeneralString(hash);
          }(path, hash);

      Sha256::Hash hash_value = Sha256::from_string(hash_string);

      printer().output().key("hash", hash_string);

      return dsa.verify(
        DigitalSignatureAlgorithm::Signature(signature),
        hash_value);
    }
  }();

  printer()
    .output()
    .key("signature", effective_signature.to_string())
    .key("publicKey", dsa.key_pair().public_key().to_string())
    .key_bool("verified", is_verified);

  if (is_verified == false) {
    if (path.is_empty() == false) {
      APP_RETURN_ASSIGN_ERROR("failed to verify " | path);
    }

    APP_RETURN_ASSIGN_ERROR("failed to verify " | hash);
  }

  if (is_error()) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to download user `" + identifier + "`");
  }

  if (!strip.is_empty()) {
    if (!is_signed) {
      APP_RETURN_ASSIGN_ERROR("Cannot strip signature. File is not signed");
    }

    const auto path_suffix = Path::suffix(path);
    if (path_suffix != strip) {
      APP_RETURN_ASSIGN_ERROR(
        "Cannot strip signature. File suffix `" | path_suffix
        | "` does not match strip suffix `" | strip | "`.");
    }

    const auto output = Path::no_suffix(path);
    const auto input_file = File(path);
    const auto original_size = input_file.size();
    const auto stripped_size = original_size - sizeof(auth_signature_marker_t);
    File(File::IsOverwrite::yes, output)
      .write(input_file, File::Write().set_size(stripped_size));
  }

  return is_success();
}

bool KeysGroup::revoke(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(revoke, "revokes the specified keys.")
      + GROUP_ARG_REQ(key_id, string, <current>, "the key id to revoke."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  StringView identifier = command.get_argument_value("key");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  if (identifier.is_empty()) {
    identifier = cloud_service().cloud().credentials().get_uid();
  }

  SlPrinter::Output printer_output_guard(printer());

  Keys keys_document(identifier);

  if (keys_document.is_valid() == false) {
    APP_RETURN_ASSIGN_ERROR(
      "the keys for id `" | identifier | "` do not exist");
  }

  if (is_error()) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to download user `" + identifier + "`");
  }

  if (keys_document.get_status() != "revoked") {
    keys_document.set_status("revoked").save();
  }

  printer().object(identifier, keys_document.to_object());

  return is_success();
}

bool KeysGroup::remove(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(remove, "removes (deletes) the key.")
      + GROUP_ARG_REQ(key_id, string, <current>, "the key id to remove."));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto identifier = command.get_argument_value("key");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());

  Keys(identifier).remove();

  if (is_error()) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to remove key `" + identifier + "`");
  }

  printer().key(identifier, "removed");

  return is_success();
}

bool KeysGroup::download(const Command &command) {

  printer().open_command(GROUP_COMMAND_NAME);
  GROUP_ADD_SESSION_REPORT_TAG();

  Command reference(
    Command::Group(get_name()),
    GROUP_ARG_DESC(download, "downloads the key and saves it to a JSON file.")
      + GROUP_ARG_REQ(key_id, string, <current>, "the key id to remove.")
      + GROUP_ARG_OPT(
        path_dest,
        string,
        <current directory>,
        "the host destination path for the key"));

  if (!command.is_valid(reference, printer())) {
    return printer().close_fail();
  }

  const auto identifier = command.get_argument_value("key");
  const auto path = command.get_argument_value("path");

  command.print_options(printer());

  if (is_cloud_ready() == false) {
    return printer().close_fail();
  }

  SlPrinter::Output printer_output_guard(printer());
  SlPrinter::Object id_object(printer().output(), identifier);

  PathString output_path;
  if (path.is_empty() == false) {
    api::ErrorScope error_scope;
    const auto info = FileSystem().get_info(path);
    if (is_error()) {
      reset_error();
      const auto parent_path = Path::parent_directory(path);
      if (
        parent_path.is_empty() == false
        && FileSystem().directory_exists(path) == false) {
        APP_RETURN_ASSIGN_ERROR(
          "the parent directory for `" + path + "` does not exist");
      }
      output_path = path;
    }
    if (info.is_directory()) {
      output_path = path / identifier & ".json";
    } else if (info.is_file()) {
      output_path = path;
    }
  } else {
    output_path = identifier & ".json";
  }

  printer()
    .key("output", output_path)
    .key_bool("overwrite", FileSystem().exists(output_path));

  Keys(identifier).export_file(File(File::IsOverwrite::yes, output_path));

  if (is_error()) {
    SL_PRINTER_TRACE("document traffic " + cloud_service().store().traffic());
    APP_RETURN_ASSIGN_ERROR("failed to download key `" + identifier + "`");
  }

  printer().key("id", identifier);

  return is_success();
}
