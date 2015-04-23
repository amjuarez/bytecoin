// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "LegacyKeysImporter.h"

#include <vector>
#include <system_error>

#include "cryptonote_core/Currency.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/AccountKVSerialization.h"
#include "file_io_utils.h"

#include "serialization/binary_utils.h"
#include "storages/portable_storage.h"
#include "storages/portable_storage_template_helper.h"

#include "wallet/wallet_errors.h"
#include "wallet/WalletSerializer.h"
#include "wallet/WalletUserTransactionsCache.h"
#include "wallet/WalletErrors.h"

namespace {

struct keys_file_data {
  crypto::chacha8_iv iv;
  std::string account_data;

  BEGIN_SERIALIZE_OBJECT()
    FIELD(iv)
    FIELD(account_data)
    END_SERIALIZE()
};

bool verify_keys(const crypto::secret_key& sec, const crypto::public_key& expected_pub) {
  crypto::public_key pub;
  bool r = crypto::secret_key_to_public_key(sec, pub);
  return r && expected_pub == pub;
}

void loadKeysFromFile(const std::string& filename, const std::string& password, cryptonote::account_base& account) {
  keys_file_data keys_file_data;
  std::string buf;
  bool r = epee::file_io_utils::load_file_to_string(filename, buf);
  THROW_WALLET_EXCEPTION_IF(!r, tools::error::file_read_error, filename);
  r = ::serialization::parse_binary(buf, keys_file_data);
  THROW_WALLET_EXCEPTION_IF(!r, tools::error::wallet_internal_error, "internal error: failed to deserialize \"" + filename + '\"');

  crypto::chacha8_key key;
  crypto::cn_context cn_context;
  crypto::generate_chacha8_key(cn_context, password, key);
  std::string account_data;
  account_data.resize(keys_file_data.account_data.size());
  crypto::chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);

  const ::cryptonote::account_keys& keys = account.get_keys();
  cryptonote::AccountBaseSerializer<false> accountSerializer(account);
  r = epee::serialization::load_t_from_binary(accountSerializer, account_data);
  r = r && verify_keys(keys.m_view_secret_key, keys.m_account_address.m_viewPublicKey);
  r = r && verify_keys(keys.m_spend_secret_key, keys.m_account_address.m_spendPublicKey);
  THROW_WALLET_EXCEPTION_IF(!r, tools::error::invalid_password);
}

}

namespace cryptonote {

void importLegacyKeys(const std::string& legacyKeysFilename, const std::string& password, std::ostream& destination) {
  cryptonote::account_base account;

  try {
    loadKeysFromFile(legacyKeysFilename, password, account);
  } catch (tools::error::invalid_password&) {
    throw std::system_error(make_error_code(cryptonote::error::WRONG_PASSWORD));
  }

  CryptoNote::WalletUserTransactionsCache transactionsCache;
  std::string cache;
  CryptoNote::WalletSerializer importer(account, transactionsCache);
  importer.serialize(destination, password, false, cache);
}

} //namespace cryptonote
