// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "TransactionExtra.h"

#include "Common/int-util.h"
#include "Common/MemoryInputStream.h"
#include "Common/StreamTools.h"
#include "Common/StringTools.h"
#include "Common/Varint.h"
#include "CryptoNoteTools.h"
#include "Serialization/BinaryOutputStreamSerializer.h"
#include "Serialization/BinaryInputStreamSerializer.h"

using namespace Crypto;
using namespace Common;

namespace CryptoNote {

bool parseTransactionExtra(const std::vector<uint8_t> &transactionExtra, std::vector<TransactionExtraField> &transactionExtraFields) {
  transactionExtraFields.clear();

  if (transactionExtra.empty())
    return true;

  try {
    MemoryInputStream iss(transactionExtra.data(), transactionExtra.size());
    BinaryInputStreamSerializer ar(iss);

    int c = 0;

    while (!iss.endOfStream()) {
      c = read<uint8_t>(iss);
      switch (c) {
      case TX_EXTRA_TAG_PADDING: {
        size_t size = 1;
        for (; !iss.endOfStream() && size <= TX_EXTRA_PADDING_MAX_COUNT; ++size) {
          if (read<uint8_t>(iss) != 0) {
            return false; // all bytes should be zero
          }
        }

        if (size > TX_EXTRA_PADDING_MAX_COUNT) {
          return false;
        }

        transactionExtraFields.push_back(TransactionExtraPadding{ size });
        break;
      }

      case TX_EXTRA_TAG_PUBKEY: {
        TransactionExtraPublicKey extraPk;
        ar(extraPk.publicKey, "public_key");
        transactionExtraFields.push_back(extraPk);
        break;
      }

      case TX_EXTRA_NONCE: {
        TransactionExtraNonce extraNonce;
        uint8_t size = read<uint8_t>(iss);
        if (size > 0) {
          extraNonce.nonce.resize(size);
          read(iss, extraNonce.nonce.data(), extraNonce.nonce.size());
        }

        transactionExtraFields.push_back(extraNonce);
        break;
      }

      case TX_EXTRA_MERGE_MINING_TAG: {
        TransactionExtraMergeMiningTag mmTag;
        ar(mmTag, "mm_tag");
        transactionExtraFields.push_back(mmTag);
        break;
      }

      case TX_EXTRA_MESSAGE_TAG: {
        tx_extra_message message;
        ar(message.data, "message");
        transactionExtraFields.push_back(message);
        break;
      }

      case TX_EXTRA_TTL: {
        uint8_t size;
        readVarint(iss, size);
        TransactionExtraTTL ttl;
        readVarint(iss, ttl.ttl);
        transactionExtraFields.push_back(ttl);
        break;
      }
      }
    }
  } catch (std::exception &) {
    return false;
  }

  return true;
}

struct ExtraSerializerVisitor : public boost::static_visitor<bool> {
  std::vector<uint8_t>& extra;

  ExtraSerializerVisitor(std::vector<uint8_t>& tx_extra)
    : extra(tx_extra) {}

  bool operator()(const TransactionExtraPadding& t) {
    if (t.size > TX_EXTRA_PADDING_MAX_COUNT) {
      return false;
    }
    extra.insert(extra.end(), t.size, 0);
    return true;
  }

  bool operator()(const TransactionExtraPublicKey& t) {
    return addTransactionPublicKeyToExtra(extra, t.publicKey);
  }

  bool operator()(const TransactionExtraNonce& t) {
    return addExtraNonceToTransactionExtra(extra, t.nonce);
  }

  bool operator()(const TransactionExtraMergeMiningTag& t) {
    return appendMergeMiningTagToExtra(extra, t);
  }

  bool operator()(const tx_extra_message& t) {
    return append_message_to_extra(extra, t);
  }

  bool operator()(const TransactionExtraTTL& t) {
    appendTTLToExtra(extra, t.ttl);
    return true;
  }
};

bool writeTransactionExtra(std::vector<uint8_t>& tx_extra, const std::vector<TransactionExtraField>& tx_extra_fields) {
  ExtraSerializerVisitor visitor(tx_extra);

  for (const auto& tag : tx_extra_fields) {
    if (!boost::apply_visitor(visitor, tag)) {
      return false;
    }
  }

  return true;
}

PublicKey getTransactionPublicKeyFromExtra(const std::vector<uint8_t>& tx_extra) {
  std::vector<TransactionExtraField> tx_extra_fields;
  parseTransactionExtra(tx_extra, tx_extra_fields);

  TransactionExtraPublicKey pub_key_field;
  if (!findTransactionExtraFieldByType(tx_extra_fields, pub_key_field))
    return boost::value_initialized<PublicKey>();

  return pub_key_field.publicKey;
}

bool addTransactionPublicKeyToExtra(std::vector<uint8_t>& tx_extra, const PublicKey& tx_pub_key) {
  tx_extra.resize(tx_extra.size() + 1 + sizeof(PublicKey));
  tx_extra[tx_extra.size() - 1 - sizeof(PublicKey)] = TX_EXTRA_TAG_PUBKEY;
  *reinterpret_cast<PublicKey*>(&tx_extra[tx_extra.size() - sizeof(PublicKey)]) = tx_pub_key;
  return true;
}


bool addExtraNonceToTransactionExtra(std::vector<uint8_t>& tx_extra, const BinaryArray& extra_nonce) {
  if (extra_nonce.size() > TX_EXTRA_NONCE_MAX_COUNT) {
    return false;
  }

  size_t start_pos = tx_extra.size();
  tx_extra.resize(tx_extra.size() + 2 + extra_nonce.size());
  //write tag
  tx_extra[start_pos] = TX_EXTRA_NONCE;
  //write len
  ++start_pos;
  tx_extra[start_pos] = static_cast<uint8_t>(extra_nonce.size());
  //write data
  ++start_pos;
  memcpy(&tx_extra[start_pos], extra_nonce.data(), extra_nonce.size());
  return true;
}

bool appendMergeMiningTagToExtra(std::vector<uint8_t>& tx_extra, const TransactionExtraMergeMiningTag& mm_tag) {
  BinaryArray blob;
  if (!toBinaryArray(mm_tag, blob)) {
    return false;
  }

  tx_extra.push_back(TX_EXTRA_MERGE_MINING_TAG);
  std::copy(reinterpret_cast<const uint8_t*>(blob.data()), reinterpret_cast<const uint8_t*>(blob.data() + blob.size()), std::back_inserter(tx_extra));
  return true;
}

bool getMergeMiningTagFromExtra(const std::vector<uint8_t>& tx_extra, TransactionExtraMergeMiningTag& mm_tag) {
  std::vector<TransactionExtraField> tx_extra_fields;
  parseTransactionExtra(tx_extra, tx_extra_fields);

  return findTransactionExtraFieldByType(tx_extra_fields, mm_tag);
}

bool append_message_to_extra(std::vector<uint8_t>& tx_extra, const tx_extra_message& message) {
  BinaryArray blob;
  if (!toBinaryArray(message, blob)) {
    return false;
  }

  tx_extra.reserve(tx_extra.size() + 1 + blob.size());
  tx_extra.push_back(TX_EXTRA_MESSAGE_TAG);
  std::copy(reinterpret_cast<const uint8_t*>(blob.data()), reinterpret_cast<const uint8_t*>(blob.data() + blob.size()), std::back_inserter(tx_extra));

  return true;
}

std::vector<std::string> get_messages_from_extra(const std::vector<uint8_t> &extra, const Crypto::PublicKey &txkey, const Crypto::SecretKey *recepient_secret_key) {
  std::vector<TransactionExtraField> tx_extra_fields;
  std::vector<std::string> result;
  if (!parseTransactionExtra(extra, tx_extra_fields)) {
    return result;
  }
  size_t i = 0;
  for (const auto& f : tx_extra_fields) {
    if (f.type() != typeid(tx_extra_message)) {
      continue;
    }
    std::string res;
    if (boost::get<tx_extra_message>(f).decrypt(i, txkey, recepient_secret_key, res)) {
      result.push_back(res);
    }
    ++i;
  }
  return result;
}

void appendTTLToExtra(std::vector<uint8_t>& tx_extra, uint64_t ttl) {
  std::string ttlData = Tools::get_varint_data(ttl);
  std::string extraFieldSize = Tools::get_varint_data(ttlData.size());

  tx_extra.reserve(tx_extra.size() + 1 + extraFieldSize.size() + ttlData.size());
  tx_extra.push_back(TX_EXTRA_TTL);
  std::copy(extraFieldSize.begin(), extraFieldSize.end(), std::back_inserter(tx_extra));
  std::copy(ttlData.begin(), ttlData.end(), std::back_inserter(tx_extra));
}

void setPaymentIdToTransactionExtraNonce(std::vector<uint8_t>& extra_nonce, const Hash& payment_id) {
  extra_nonce.clear();
  extra_nonce.push_back(TX_EXTRA_NONCE_PAYMENT_ID);
  const uint8_t* payment_id_ptr = reinterpret_cast<const uint8_t*>(&payment_id);
  std::copy(payment_id_ptr, payment_id_ptr + sizeof(payment_id), std::back_inserter(extra_nonce));
}

bool getPaymentIdFromTransactionExtraNonce(const std::vector<uint8_t>& extra_nonce, Hash& payment_id) {
  if (sizeof(Hash) + 1 != extra_nonce.size())
    return false;
  if (TX_EXTRA_NONCE_PAYMENT_ID != extra_nonce[0])
    return false;
  payment_id = *reinterpret_cast<const Hash*>(extra_nonce.data() + 1);
  return true;
}

bool parsePaymentId(const std::string& paymentIdString, Hash& paymentId) {
  return Common::podFromHex(paymentIdString, paymentId);
}

bool createTxExtraWithPaymentId(const std::string& paymentIdString, std::vector<uint8_t>& extra) {
  Hash paymentIdBin;

  if (!parsePaymentId(paymentIdString, paymentIdBin)) {
    return false;
  }

  std::vector<uint8_t> extraNonce;
  CryptoNote::setPaymentIdToTransactionExtraNonce(extraNonce, paymentIdBin);

  if (!CryptoNote::addExtraNonceToTransactionExtra(extra, extraNonce)) {
    return false;
  }

  return true;
}

bool getPaymentIdFromTxExtra(const std::vector<uint8_t>& extra, Hash& paymentId) {
  std::vector<TransactionExtraField> tx_extra_fields;
  if (!parseTransactionExtra(extra, tx_extra_fields)) {
    return false;
  }

  TransactionExtraNonce extra_nonce;
  if (findTransactionExtraFieldByType(tx_extra_fields, extra_nonce)) {
    if (!getPaymentIdFromTransactionExtraNonce(extra_nonce.nonce, paymentId)) {
      return false;
    }
  } else {
    return false;
  }

  return true;
}

#define TX_EXTRA_MESSAGE_CHECKSUM_SIZE 4

#pragma pack(push, 1)
struct message_key_data {
  KeyDerivation derivation;
  uint8_t magic1, magic2;
};
#pragma pack(pop)
static_assert(sizeof(message_key_data) == 34, "Invalid structure size");

bool tx_extra_message::encrypt(size_t index, const std::string &message, const AccountPublicAddress *recipient, const KeyPair &txkey) {
  size_t mlen = message.size();
  std::unique_ptr<char[]> buf(new char[mlen + TX_EXTRA_MESSAGE_CHECKSUM_SIZE]);
  memcpy(buf.get(), message.data(), mlen);
  memset(buf.get() + mlen, 0, TX_EXTRA_MESSAGE_CHECKSUM_SIZE);
  mlen += TX_EXTRA_MESSAGE_CHECKSUM_SIZE;
  if (recipient) {
    message_key_data key_data;
    if (!generate_key_derivation(recipient->spendPublicKey, txkey.secretKey, key_data.derivation)) {
      return false;
    }
    key_data.magic1 = 0x80;
    key_data.magic2 = 0;
    Hash h = cn_fast_hash(&key_data, sizeof(message_key_data));
    uint64_t nonce = SWAP64LE(index);
    chacha(10, buf.get(), mlen, reinterpret_cast<uint8_t *>(&h), reinterpret_cast<uint8_t *>(&nonce), buf.get());
  }
  data.assign(buf.get(), mlen);
  return true;
}

bool tx_extra_message::decrypt(size_t index, const Crypto::PublicKey &txkey, const Crypto::SecretKey *recepient_secret_key, std::string &message) const {
  size_t mlen = data.size();
  if (mlen < TX_EXTRA_MESSAGE_CHECKSUM_SIZE) {
    return false;
  }
  const char *buf;
  std::unique_ptr<char[]> ptr;
  if (recepient_secret_key != nullptr) {
    ptr.reset(new char[mlen]);
    assert(ptr);
    message_key_data key_data;
    if (!generate_key_derivation(txkey, *recepient_secret_key, key_data.derivation)) {
      return false;
    }
    key_data.magic1 = 0x80;
    key_data.magic2 = 0;
    Hash h = cn_fast_hash(&key_data, sizeof(message_key_data));
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

bool tx_extra_message::serialize(ISerializer& s) {
  s(data, "data");
  return true;
}

}
