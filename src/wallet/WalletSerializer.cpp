// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletSerializer.h"

#include <stdexcept>

#include "serialization/BinaryOutputStreamSerializer.h"
#include "serialization/BinaryInputStreamSerializer.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_serialization.h"
#include "WalletUserTransactionsCache.h"
#include "WalletErrors.h"
#include "KeysStorage.h"

namespace {

const uint32_t WALLET_SERIALIZATION_VERSION = 2;

bool verifyKeys(const crypto::secret_key& sec, const crypto::public_key& expected_pub) {
  crypto::public_key pub;
  bool r = crypto::secret_key_to_public_key(sec, pub);
  return r && expected_pub == pub;
}

void throwIfKeysMissmatch(const crypto::secret_key& sec, const crypto::public_key& expected_pub) {
  if (!verifyKeys(sec, expected_pub))
    throw std::system_error(make_error_code(cryptonote::error::WRONG_PASSWORD));
}

}

namespace CryptoNote {

WalletSerializer::WalletSerializer(cryptonote::account_base& account, WalletUserTransactionsCache& transactionsCache) :
  account(account),
  transactionsCache(transactionsCache),
  walletSerializationVersion(WALLET_SERIALIZATION_VERSION)
{
}

void WalletSerializer::serialize(std::ostream& stream, const std::string& password, bool saveDetailed, const std::string& cache) {
  std::stringstream plainArchive;
  cryptonote::BinaryOutputStreamSerializer serializer(plainArchive);
  saveKeys(serializer);

  serializer(saveDetailed, "has_details");

  if (saveDetailed) {
    serializer(transactionsCache, "details");
  }

  serializer.binary(const_cast<std::string&>(cache), "cache");

  std::string plain = plainArchive.str();
  std::string cipher;

  crypto::chacha_iv iv = encrypt(plain, password, cipher);

  uint32_t version = walletSerializationVersion;
  cryptonote::BinaryOutputStreamSerializer s(stream);
  s.beginObject("wallet");
  s(version, "version");
  s(iv, "iv");
  s(cipher, "data");
  s.endObject();
}

void WalletSerializer::saveKeys(cryptonote::ISerializer& serializer) {
  cryptonote::KeysStorage keys;
  cryptonote::account_keys acc = account.get_keys();

  keys.creationTimestamp = account.get_createtime();
  keys.spendPublicKey = acc.m_account_address.m_spendPublicKey;
  keys.spendSecretKey = acc.m_spend_secret_key;
  keys.viewPublicKey = acc.m_account_address.m_viewPublicKey;
  keys.viewSecretKey = acc.m_view_secret_key;

  keys.serialize(serializer, "keys");
}

crypto::chacha_iv WalletSerializer::encrypt(const std::string& plain, const std::string& password, std::string& cipher) {
  crypto::chacha_key key;
  crypto::cn_context context;
  crypto::generate_chacha8_key(context, password, key);

  cipher.resize(plain.size());

  crypto::chacha_iv iv = crypto::rand<crypto::chacha_iv>();
  crypto::chacha8(plain.data(), plain.size(), key, iv, &cipher[0]);

  return iv;
}


void WalletSerializer::deserialize(std::istream& stream, const std::string& password, std::string& cache) {
  cryptonote::BinaryInputStreamSerializer serializerEncrypted(stream);

  serializerEncrypted.beginObject("wallet");

  uint32_t version;
  serializerEncrypted(version, "version");

  crypto::chacha_iv iv;
  serializerEncrypted(iv, "iv");

  std::string cipher;
  serializerEncrypted(cipher, "data");

  serializerEncrypted.endObject();

  std::string plain;
  decrypt(cipher, plain, iv, password);

  std::stringstream decryptedStream(plain);

  cryptonote::BinaryInputStreamSerializer serializer(decryptedStream);

  try
  {
    loadKeys(serializer);
    throwIfKeysMissmatch(account.get_keys().m_view_secret_key, account.get_keys().m_account_address.m_viewPublicKey);
    throwIfKeysMissmatch(account.get_keys().m_spend_secret_key, account.get_keys().m_account_address.m_spendPublicKey);
  }
  catch (std::exception&) {
    throw std::system_error(make_error_code(cryptonote::error::WRONG_PASSWORD));
  }

  bool detailsSaved;

  serializer(detailsSaved, "has_details");

  if (detailsSaved) {
    if (version == 1) {
      transactionsCache.deserializeLegacyV1(serializer, "details");
    } else {
      serializer(transactionsCache, "details");
    }
  }

  serializer.binary(cache, "cache");
}

void WalletSerializer::decrypt(const std::string& cipher, std::string& plain, crypto::chacha_iv iv, const std::string& password) {
  crypto::chacha_key key;
  crypto::cn_context context;
  crypto::generate_chacha8_key(context, password, key);

  plain.resize(cipher.size());

  crypto::chacha8(cipher.data(), cipher.size(), key, iv, &plain[0]);
}

void WalletSerializer::loadKeys(cryptonote::ISerializer& serializer) {
  cryptonote::KeysStorage keys;

  keys.serialize(serializer, "keys");

  cryptonote::account_keys acc;
  acc.m_account_address.m_spendPublicKey = keys.spendPublicKey;
  acc.m_spend_secret_key = keys.spendSecretKey;
  acc.m_account_address.m_viewPublicKey = keys.viewPublicKey;
  acc.m_view_secret_key = keys.viewSecretKey;

  account.set_keys(acc);
  account.set_createtime(keys.creationTimestamp);
}

}


