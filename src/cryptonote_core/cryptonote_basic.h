// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <vector>

#include <boost/utility/value_init.hpp>
#include <boost/variant.hpp>

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "cryptonote_core/tx_extra.h"
#include "serialization/binary_archive.h"
#include "serialization/crypto.h"
#include "serialization/keyvalue_serialization.h" // eepe named serialization
#include "serialization/debug_archive.h"
#include "serialization/json_archive.h"
#include "serialization/serialization.h"
#include "serialization/variant.h"
#include "cryptonote_config.h"

namespace cryptonote {
  class account_base;
  struct account_keys;
  struct Block;
  struct Transaction;

  // Implemented in cryptonote_format_utils.cpp
  bool get_transaction_hash(const Transaction& t, crypto::hash& res);

  const static crypto::hash null_hash = boost::value_initialized<crypto::hash>();
  const static crypto::public_key null_pkey = boost::value_initialized<crypto::public_key>();

  /* inputs */

  struct TransactionInputGenerate {
    size_t height;

    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(height);
    END_SERIALIZE()
  };

  struct TransactionInputToKey {
    uint64_t amount;
    std::vector<uint64_t> keyOffsets;
    crypto::key_image keyImage;      // double spending protection

    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(amount);
      FIELD(keyOffsets);
      FIELD(keyImage);
    END_SERIALIZE()
  };

  struct TransactionInputMultisignature {
    uint64_t amount;
    uint32_t signatures;
    uint64_t outputIndex;

    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(amount);
      VARINT_FIELD(signatures);
      VARINT_FIELD(outputIndex);
    END_SERIALIZE()
  };

  /* outputs */

  struct TransactionOutputToKey {
    TransactionOutputToKey() { }
    TransactionOutputToKey(const crypto::public_key &_key) : key(_key) { }
    crypto::public_key key;
  };

  struct TransactionOutputMultisignature {
    std::vector<crypto::public_key> keys;
    uint32_t requiredSignatures;

    BEGIN_SERIALIZE_OBJECT()
      FIELD(keys);
      VARINT_FIELD(requiredSignatures);
    END_SERIALIZE()
  };

  struct TransactionInputToScript {
    BEGIN_SERIALIZE_OBJECT()
    END_SERIALIZE()
  };

  struct TransactionInputToScriptHash {
    BEGIN_SERIALIZE_OBJECT()
    END_SERIALIZE()
  };

  struct TransactionOutputToScript {
    BEGIN_SERIALIZE_OBJECT()
    END_SERIALIZE()
  };

  struct TransactionOutputToScriptHash {
    BEGIN_SERIALIZE_OBJECT()
    END_SERIALIZE()
  };

  typedef boost::variant<
    TransactionInputGenerate,
    TransactionInputToScript,
    TransactionInputToScriptHash,
    TransactionInputToKey,
    TransactionInputMultisignature> TransactionInput;

  typedef boost::variant<
    TransactionOutputToScript,
    TransactionOutputToScriptHash,
    TransactionOutputToKey,
    TransactionOutputMultisignature> TransactionOutputTarget;

  struct TransactionOutput {
    uint64_t amount;
    TransactionOutputTarget target;

    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(amount);
      FIELD(target);
    END_SERIALIZE()
  };

  struct TransactionPrefix {
    // tx information
    size_t   version;
    uint64_t unlockTime;  //number of block (or time), used as a limitation like: spend this tx not early then block/time

    std::vector<TransactionInput> vin;
    std::vector<TransactionOutput> vout;
    //extra
    std::vector<uint8_t> extra;

    BEGIN_SERIALIZE()
      VARINT_FIELD(version);
      if(CURRENT_TRANSACTION_VERSION < version) {
        return false;
      }
      VARINT_FIELD(unlockTime);
      FIELD(vin);
      FIELD(vout);
      FIELD(extra);
    END_SERIALIZE()

  protected:
    TransactionPrefix() {}
  };

  struct Transaction: public TransactionPrefix {
    std::vector<std::vector<crypto::signature> > signatures; //count signatures  always the same as inputs count

    Transaction() {
      clear();
    }

    void clear() {
      version = 0;
      unlockTime = 0;
      vin.clear();
      vout.clear();
      extra.clear();
      signatures.clear();
    }

    BEGIN_SERIALIZE_OBJECT()
      FIELDS(*static_cast<TransactionPrefix *>(this))

      ar.tag("signatures");
      ar.begin_array();
      PREPARE_CUSTOM_VECTOR_SERIALIZATION(vin.size(), signatures);
      bool signatures_not_expected = signatures.empty();
      if (!signatures_not_expected && vin.size() != signatures.size()) {
        return false;
      }

      for (size_t i = 0; i < vin.size(); ++i) {
        size_t signatureSize = getSignatureSize(vin[i]);
        if (signatures_not_expected) {
          if (0 == signatureSize) {
            continue;
          } else {
            return false;
          }
        }

        PREPARE_CUSTOM_VECTOR_SERIALIZATION(signatureSize, signatures[i]);
        if (signatureSize != signatures[i].size()) {
          return false;
        }

        FIELDS(signatures[i]);

        if (vin.size() - i > 1) {
          ar.delimit_array();
        }
      }
      ar.end_array();
    END_SERIALIZE()

    static size_t getSignatureSize(const TransactionInput& input) {
      struct txin_signature_size_visitor : public boost::static_visitor<size_t> {
        size_t operator()(const TransactionInputGenerate&       txin) const { return 0; }
        size_t operator()(const TransactionInputToScript&       txin) const { assert(false); return 0; }
        size_t operator()(const TransactionInputToScriptHash&   txin) const { assert(false); return 0; }
        size_t operator()(const TransactionInputToKey&          txin) const { return txin.keyOffsets.size();}
        size_t operator()(const TransactionInputMultisignature& txin) const { return txin.signatures; }
      };

      return boost::apply_visitor(txin_signature_size_visitor(), input);
    }
  };

  struct BlockHeader {
    uint8_t majorVersion;
    uint8_t minorVersion;
    uint32_t nonce;
    uint64_t timestamp;
    crypto::hash prevId;

    BEGIN_SERIALIZE()
      VARINT_FIELD(majorVersion)
      if (majorVersion > BLOCK_MAJOR_VERSION_1) {
        return false;
      }
      VARINT_FIELD(minorVersion)
      VARINT_FIELD(timestamp);
      FIELD(prevId);
      FIELD(nonce);
    END_SERIALIZE()
  };

  struct Block: public BlockHeader {
    Transaction minerTx;
    std::vector<crypto::hash> txHashes;

    BEGIN_SERIALIZE_OBJECT()
      FIELDS(*static_cast<BlockHeader *>(this));
      FIELD(minerTx);
      FIELD(txHashes);
    END_SERIALIZE()
  };

  struct AccountPublicAddress {
    crypto::public_key m_spendPublicKey;
    crypto::public_key m_viewPublicKey;

    BEGIN_SERIALIZE_OBJECT()
      FIELD(m_spendPublicKey);
      FIELD(m_viewPublicKey);
    END_SERIALIZE()
  };

  struct KeyPair {
    crypto::public_key pub;
    crypto::secret_key sec;

    static KeyPair generate() {
      KeyPair k;
      generate_keys(k.pub, k.sec);
      return k;
    }
  };
}

BLOB_SERIALIZER(cryptonote::TransactionOutputToKey);

VARIANT_TAG(binary_archive, cryptonote::TransactionInputGenerate, 0xff);
VARIANT_TAG(binary_archive, cryptonote::TransactionInputToScript, 0x0);
VARIANT_TAG(binary_archive, cryptonote::TransactionInputToScriptHash, 0x1);
VARIANT_TAG(binary_archive, cryptonote::TransactionInputToKey, 0x2);
VARIANT_TAG(binary_archive, cryptonote::TransactionInputMultisignature, 0x3);
VARIANT_TAG(binary_archive, cryptonote::TransactionOutputToScript, 0x0);
VARIANT_TAG(binary_archive, cryptonote::TransactionOutputToScriptHash, 0x1);
VARIANT_TAG(binary_archive, cryptonote::TransactionOutputToKey, 0x2);
VARIANT_TAG(binary_archive, cryptonote::TransactionOutputMultisignature, 0x3);
VARIANT_TAG(binary_archive, cryptonote::Transaction, 0xcc);
VARIANT_TAG(binary_archive, cryptonote::Block, 0xbb);

VARIANT_TAG(json_archive, cryptonote::TransactionInputGenerate, "generate");
VARIANT_TAG(json_archive, cryptonote::TransactionInputToScript, "script");
VARIANT_TAG(json_archive, cryptonote::TransactionInputToScriptHash, "scripthash");
VARIANT_TAG(json_archive, cryptonote::TransactionInputToKey, "key");
VARIANT_TAG(json_archive, cryptonote::TransactionInputMultisignature, "multisignature");
VARIANT_TAG(json_archive, cryptonote::TransactionOutputToScript, "script");
VARIANT_TAG(json_archive, cryptonote::TransactionOutputToScriptHash, "scripthash");
VARIANT_TAG(json_archive, cryptonote::TransactionOutputToKey, "key");
VARIANT_TAG(json_archive, cryptonote::TransactionOutputMultisignature, "multisignature");
VARIANT_TAG(json_archive, cryptonote::Transaction, "Transaction");
VARIANT_TAG(json_archive, cryptonote::Block, "Block");

VARIANT_TAG(debug_archive, cryptonote::TransactionInputGenerate, "generate");
VARIANT_TAG(debug_archive, cryptonote::TransactionInputToScript, "script");
VARIANT_TAG(debug_archive, cryptonote::TransactionInputToScriptHash, "scripthash");
VARIANT_TAG(debug_archive, cryptonote::TransactionInputToKey, "key");
VARIANT_TAG(debug_archive, cryptonote::TransactionInputMultisignature, "multisignature");
VARIANT_TAG(debug_archive, cryptonote::TransactionOutputToScript, "script");
VARIANT_TAG(debug_archive, cryptonote::TransactionOutputToScriptHash, "scripthash");
VARIANT_TAG(debug_archive, cryptonote::TransactionOutputToKey, "key");
VARIANT_TAG(debug_archive, cryptonote::TransactionOutputMultisignature, "multisignature");
VARIANT_TAG(debug_archive, cryptonote::Transaction, "Transaction");
VARIANT_TAG(debug_archive, cryptonote::Block, "Block");
