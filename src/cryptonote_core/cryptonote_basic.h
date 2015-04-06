// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
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
  struct tx_extra_merge_mining_tag;

  // Implemented in cryptonote_format_utils.cpp
  bool get_transaction_hash(const Transaction& t, crypto::hash& res);
  bool get_mm_tag_from_extra(const std::vector<uint8_t>& tx, tx_extra_merge_mining_tag& mm_tag);

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

  struct ParentBlock {
    uint8_t majorVersion;
    uint8_t minorVersion;
    crypto::hash prevId;
    size_t numberOfTransactions;
    std::vector<crypto::hash> minerTxBranch;
    Transaction minerTx;
    std::vector<crypto::hash> blockchainBranch;
  };

  struct ParentBlockSerializer {
    ParentBlockSerializer(ParentBlock& parentBlock, uint64_t& timestamp, uint32_t& nonce, bool hashingSerialization, bool headerOnly) :
      m_parentBlock(parentBlock), m_timestamp(timestamp), m_nonce(nonce), m_hashingSerialization(hashingSerialization), m_headerOnly(headerOnly) {
    }

    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD_N("majorVersion", m_parentBlock.majorVersion);
      if (BLOCK_MAJOR_VERSION_1 < m_parentBlock.majorVersion) {
        return false;
      }
      VARINT_FIELD_N("minorVersion", m_parentBlock.minorVersion);
      VARINT_FIELD_N("timestamp", m_timestamp);
      FIELD_N("prevId", m_parentBlock.prevId);
      FIELD_N("nonce", m_nonce);

      if (m_hashingSerialization) {
        crypto::hash minerTxHash;
        if (!get_transaction_hash(m_parentBlock.minerTx, minerTxHash)) {
          return false;
        }

        crypto::hash merkleRoot;
        crypto::tree_hash_from_branch(m_parentBlock.minerTxBranch.data(), m_parentBlock.minerTxBranch.size(), minerTxHash, 0, merkleRoot);

        FIELD(merkleRoot);
      }

      VARINT_FIELD_N("numberOfTransactions", m_parentBlock.numberOfTransactions);
      if (m_parentBlock.numberOfTransactions < 1) {
        return false;
      }

      if (!m_headerOnly) {
        ar.tag("minerTxBranch");
        ar.begin_array();
        size_t branchSize = crypto::tree_depth(m_parentBlock.numberOfTransactions);
        PREPARE_CUSTOM_VECTOR_SERIALIZATION(branchSize, const_cast<ParentBlock&>(m_parentBlock).minerTxBranch);
        if (m_parentBlock.minerTxBranch.size() != branchSize) {
          return false;
        }
        for (size_t i = 0; i < branchSize; ++i) {
          FIELDS(m_parentBlock.minerTxBranch[i]);
          if (i + 1 < branchSize) {
            ar.delimit_array();
          }
        }
        ar.end_array();

        FIELD(m_parentBlock.minerTx);

        tx_extra_merge_mining_tag mmTag;
        if (!get_mm_tag_from_extra(m_parentBlock.minerTx.extra, mmTag)) {
          return false;
        }

        if (mmTag.depth > 8 * sizeof(crypto::hash)) {
          return false;
        }

        ar.tag("blockchainBranch");
        ar.begin_array();
        PREPARE_CUSTOM_VECTOR_SERIALIZATION(mmTag.depth, const_cast<ParentBlock&>(m_parentBlock).blockchainBranch);
        if (mmTag.depth != m_parentBlock.blockchainBranch.size()) {
          return false;
        }
        for (size_t i = 0; i < mmTag.depth; ++i) {
          FIELDS(m_parentBlock.blockchainBranch[i]);
          if (i + 1 < mmTag.depth) {
            ar.delimit_array();
          }
        }
        ar.end_array();
      }
    END_SERIALIZE()

    ParentBlock& m_parentBlock;
    uint64_t& m_timestamp;
    uint32_t& m_nonce;
    bool m_hashingSerialization;
    bool m_headerOnly;
  };

  // Implemented below
  inline ParentBlockSerializer makeParentBlockSerializer(const Block& b, bool hashingSerialization, bool headerOnly);

  struct BlockHeader {
    uint8_t majorVersion;
    uint8_t minorVersion;
    uint32_t nonce;
    uint64_t timestamp;
    crypto::hash prevId;

    BEGIN_SERIALIZE()
      VARINT_FIELD(majorVersion)
      if (majorVersion > BLOCK_MAJOR_VERSION_2) {
        return false;
      }
      VARINT_FIELD(minorVersion)
      if (majorVersion == BLOCK_MAJOR_VERSION_1) {
        VARINT_FIELD(timestamp);
        FIELD(prevId);
        FIELD(nonce);
      } else if (majorVersion == BLOCK_MAJOR_VERSION_2) {
        FIELD(prevId);
      } else {
        return false;
      }
    END_SERIALIZE()
  };

  struct Block: public BlockHeader {
    ParentBlock parentBlock;

    Transaction minerTx;
    std::vector<crypto::hash> txHashes;

    BEGIN_SERIALIZE_OBJECT()
      FIELDS(*static_cast<BlockHeader *>(this));
      if (majorVersion == BLOCK_MAJOR_VERSION_2) {
        auto serializer = makeParentBlockSerializer(*this, false, false);
        FIELD_N("parentBlock", serializer);
      }
      FIELD(minerTx);
      FIELD(txHashes);
    END_SERIALIZE()
  };

  inline ParentBlockSerializer makeParentBlockSerializer(const Block& b, bool hashingSerialization, bool headerOnly) {
    Block& blockRef = const_cast<Block&>(b);
    return ParentBlockSerializer(blockRef.parentBlock, blockRef.timestamp, blockRef.nonce, hashingSerialization, headerOnly);
  }

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
