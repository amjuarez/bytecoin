// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "LegacyKeysImporter.h"

#include <vector>
#include <system_error>

#include "Common/StringTools.h"

#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

#include "Serialization/SerializationTools.h"

#include "WalletLegacy/WalletLegacySerializer.h"
#include "WalletLegacy/WalletUserTransactionsCache.h"
#include "Wallet/WalletErrors.h"

using namespace Crypto;

namespace {

struct keys_file_data {
  chacha8_iv iv;
  std::string account_data;

  void serialize(CryptoNote::ISerializer& s) {
    s(iv, "iv");
    s(account_data, "account_data");
  }
};

bool verify_keys(const SecretKey& sec, const PublicKey& expected_pub) {
  PublicKey pub;
  bool r = secret_key_to_public_key(sec, pub);
  return r && expected_pub == pub;
}

void loadKeysFromFile(const std::string& filename, const std::string& password, CryptoNote::AccountBase& account) {
  keys_file_data keys_file_data;
  std::string buf;

  if (!Common::loadFileToString(filename, buf)) {
    throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR), "failed to load \"" + filename + '\"');
  }

  if (!CryptoNote::fromBinaryArray(keys_file_data, Common::asBinaryArray(buf))) {
    throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR), "failed to deserialize \"" + filename + '\"');
  }

  chacha8_key key;
  cn_context cn_context;
  generate_chacha8_key(cn_context, password, key);
  std::string account_data;
  account_data.resize(keys_file_data.account_data.size());
  chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);

  const CryptoNote::AccountKeys& keys = account.getAccountKeys();

  if (CryptoNote::loadFromBinaryKeyValue(account, account_data) &&
      verify_keys(keys.viewSecretKey, keys.address.viewPublicKey) &&
      verify_keys(keys.spendSecretKey, keys.address.spendPublicKey)) {
    return;
  }

  throw std::system_error(make_error_code(CryptoNote::error::WRONG_PASSWORD));
}

}

namespace CryptoNote {

void importLegacyKeys(const std::string& legacyKeysFilename, const std::string& password, std::ostream& destination) {
  CryptoNote::AccountBase account;

  loadKeysFromFile(legacyKeysFilename, password, account);

  CryptoNote::WalletUserTransactionsCache transactionsCache;
  std::string cache;
  CryptoNote::WalletLegacySerializer importer(account, transactionsCache);
  importer.serialize(destination, password, false, cache);
}

} //namespace CryptoNote
