// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#include "LegacyKeysImporter.h"

#include <vector>
#include <system_error>

#include "Common/StringTools.h"

#include "cryptonote_core/Currency.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/AccountKVSerialization.h"

#include "serialization/binary_utils.h"
#include "storages/portable_storage.h"
#include "storages/portable_storage_template_helper.h"

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

void loadKeysFromFile(const std::string& filename, const std::string& password, CryptoNote::account_base& account) {
  keys_file_data keys_file_data;
  std::string buf;

  if (!Common::loadFileToString(filename, buf)) {
    throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR), "failed to load \"" + filename + '\"');
  }

  if (!::serialization::parse_binary(buf, keys_file_data)) {
    throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR), "failed to deserialize \"" + filename + '\"');
  }

  crypto::chacha8_key key;
  crypto::cn_context cn_context;
  crypto::generate_chacha8_key(cn_context, password, key);
  std::string account_data;
  account_data.resize(keys_file_data.account_data.size());
  crypto::chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);

  const ::CryptoNote::account_keys& keys = account.get_keys();
  CryptoNote::AccountBaseSerializer<false> accountSerializer(account);
  bool r = epee::serialization::load_t_from_binary(accountSerializer, account_data);
  r = r && verify_keys(keys.m_view_secret_key, keys.m_account_address.m_viewPublicKey);
  r = r && verify_keys(keys.m_spend_secret_key, keys.m_account_address.m_spendPublicKey);

  if (!r) {
    throw std::system_error(make_error_code(CryptoNote::error::WRONG_PASSWORD));
  }
}

}

namespace CryptoNote {

void importLegacyKeys(const std::string& legacyKeysFilename, const std::string& password, std::ostream& destination) {
  CryptoNote::account_base account;

  loadKeysFromFile(legacyKeysFilename, password, account);

  CryptoNote::WalletUserTransactionsCache transactionsCache;
  std::string cache;
  CryptoNote::WalletSerializer importer(account, transactionsCache);
  importer.serialize(destination, password, false, cache);
}

} //namespace CryptoNote
