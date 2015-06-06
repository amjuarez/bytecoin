// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "account.h"
#include "common/int-util.h"
#include "crypto/chacha.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "cryptonote_basic.h"
#include "tx_extra.h"

#define TX_EXTRA_MESSAGE_CHECKSUM_SIZE 4

namespace cryptonote {

  using std::memcpy;
  using std::memset;
  using std::size_t;
  using std::string;
  using std::uint8_t;
  using std::unique_ptr;

  using crypto::chacha;
  using crypto::cn_fast_hash;
  using crypto::generate_key_derivation;
  using crypto::hash;
  using crypto::key_derivation;
  using crypto::public_key;

#pragma pack(push, 1)
  struct message_key_data {
    key_derivation derivation;
    uint8_t magic1, magic2;
  };
#pragma pack(pop)
  static_assert(sizeof(message_key_data) == 34, "Invalid structure size");

  bool tx_extra_message::encrypt(size_t index, const string &message, const AccountPublicAddress *recipient, const KeyPair &txkey) {
    size_t mlen = message.size();
    unique_ptr<char[]> buf(new char[mlen + TX_EXTRA_MESSAGE_CHECKSUM_SIZE]);
    memcpy(buf.get(), message.data(), mlen);
    memset(buf.get() + mlen, 0, TX_EXTRA_MESSAGE_CHECKSUM_SIZE);
    mlen += TX_EXTRA_MESSAGE_CHECKSUM_SIZE;
    if (recipient) {
      message_key_data key_data;
      if (!generate_key_derivation(recipient->m_spendPublicKey, txkey.sec, key_data.derivation)) {
        return false;
      }
      key_data.magic1 = 0x80;
      key_data.magic2 = 0;
      hash h = cn_fast_hash(&key_data, sizeof(message_key_data));
      uint64_t nonce = SWAP64LE(index);
      chacha(10, buf.get(), mlen, reinterpret_cast<uint8_t *>(&h), reinterpret_cast<uint8_t *>(&nonce), buf.get());
    }
    data.assign(buf.get(), mlen);
    return true;
  }

  bool tx_extra_message::decrypt(size_t index, const public_key &txkey, const crypto::secret_key *recepient_secret_key, std::string &message) const {
    size_t mlen = data.size();
    if (mlen < TX_EXTRA_MESSAGE_CHECKSUM_SIZE) {
      return false;
    }
    const char *buf;
    unique_ptr<char[]> ptr;
    if (recepient_secret_key != nullptr) {
      ptr.reset(new char[mlen]);
      assert(ptr);
      message_key_data key_data;
      if (!generate_key_derivation(txkey, *recepient_secret_key, key_data.derivation)) {
        return false;
      }
      key_data.magic1 = 0x80;
      key_data.magic2 = 0;
      hash h = cn_fast_hash(&key_data, sizeof(message_key_data));
      uint64_t nonce = SWAP64LE(index);
      chacha(10, data.data(), mlen, reinterpret_cast<uint8_t *>(&h), reinterpret_cast<uint8_t *>(&nonce), ptr.get());
      buf = ptr.get();
    } else {
      buf = data.data();
    }
    mlen -= TX_EXTRA_MESSAGE_CHECKSUM_SIZE;
    for (size_t i = 0; i < TX_EXTRA_MESSAGE_CHECKSUM_SIZE; i++) {
      if (buf[mlen + i] != 0) {
        return false;
      }
    }
    message.assign(buf, mlen);
    return true;
  }
}
