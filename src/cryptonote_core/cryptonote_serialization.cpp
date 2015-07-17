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

#include "cryptonote_serialization.h"
#include "account.h"

#include "serialization/ISerializer.h"
#include "serialization/SerializationOverloads.h"
#include "serialization/BinaryInputStreamSerializer.h"
#include "serialization/BinaryOutputStreamSerializer.h"
#include "crypto/crypto.h"
#include "cryptonote_config.h"

#include <stdexcept>
#include <algorithm>

#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

namespace {

struct BinaryVariantTagGetter: boost::static_visitor<uint8_t> {
  uint8_t operator()(const CryptoNote::TransactionInputGenerate) { return  0xff; }
  uint8_t operator()(const CryptoNote::TransactionInputToScript) { return  0x0; }
  uint8_t operator()(const CryptoNote::TransactionInputToScriptHash) { return  0x1; }
  uint8_t operator()(const CryptoNote::TransactionInputToKey) { return  0x2; }
  uint8_t operator()(const CryptoNote::TransactionInputMultisignature) { return  0x3; }
  uint8_t operator()(const CryptoNote::TransactionOutputToScript) { return  0x0; }
  uint8_t operator()(const CryptoNote::TransactionOutputToScriptHash) { return  0x1; }
  uint8_t operator()(const CryptoNote::TransactionOutputToKey) { return  0x2; }
  uint8_t operator()(const CryptoNote::TransactionOutputMultisignature) { return  0x3; }
  uint8_t operator()(const CryptoNote::Transaction) { return  0xcc; }
  uint8_t operator()(const CryptoNote::Block) { return  0xbb; }
};

struct VariantSerializer : boost::static_visitor<> {
  VariantSerializer(CryptoNote::ISerializer& serializer, const std::string& name) : s(serializer), name(name) {}

  void operator() (CryptoNote::TransactionInputGenerate& param) { s(param, name); }
  void operator() (CryptoNote::TransactionInputToScript& param) { s(param, name); }
  void operator() (CryptoNote::TransactionInputToScriptHash& param) { s(param, name); }
  void operator() (CryptoNote::TransactionInputToKey& param) { s(param, name); }
  void operator() (CryptoNote::TransactionInputMultisignature& param) { s(param, name); }
  void operator() (CryptoNote::TransactionOutputToScript& param) { s(param, name); }
  void operator() (CryptoNote::TransactionOutputToScriptHash& param) { s(param, name); }
  void operator() (CryptoNote::TransactionOutputToKey& param) { s(param, name); }
  void operator() (CryptoNote::TransactionOutputMultisignature& param) { s(param, name); }

  CryptoNote::ISerializer& s;
  const std::string& name;
};

void getVariantValue(CryptoNote::ISerializer& serializer, uint8_t tag, CryptoNote::TransactionInput& in) {
  switch(tag) {
  case 0xff: {
    CryptoNote::TransactionInputGenerate v;
    serializer(v, "data");
    in = v;
    break;
  }
  case 0x0: {
    CryptoNote::TransactionInputToScript v;
    serializer(v, "data");
    in = v;
    break;
  }
  case 0x1: {
    CryptoNote::TransactionInputToScriptHash v;
    serializer(v, "data");
    in = v;
    break;
  }
  case 0x2: {
    CryptoNote::TransactionInputToKey v;
    serializer(v, "data");
    in = v;
    break;
  }
  case 0x3: {
    CryptoNote::TransactionInputMultisignature v;
    serializer(v, "data");
    in = v;
    break;
  }
  default:
    throw std::runtime_error("Unknown variant tag");
  }
}

void getVariantValue(CryptoNote::ISerializer& serializer, uint8_t tag, CryptoNote::TransactionOutputTarget& out) {
  switch(tag) {
  case 0x0: {
    CryptoNote::TransactionOutputToScript v;
    serializer(v, "data");
    out = v;
    break;
  }
  case 0x1: {
    CryptoNote::TransactionOutputToScriptHash v;
    serializer(v, "data");
    out = v;
    break;
  }
  case 0x2: {
    CryptoNote::TransactionOutputToKey v;
    serializer(v, "data");
    out = v;
    break;
  }
  case 0x3: {
    CryptoNote::TransactionOutputMultisignature v;
    serializer(v, "data");
    out = v;
    break;
  }
  default:
    throw std::runtime_error("Unknown variant tag");
  }
}

template <typename T>
bool serializePod(T& v, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializer.binary(&v, sizeof(v), name);
}

bool serializeVarintVector(std::vector<uint64_t>& vector, CryptoNote::ISerializer& serializer, Common::StringView name) {
  std::size_t size = vector.size();
  
  if (!serializer.beginArray(size, name)) {
    vector.clear();
    return false;
  }

  vector.resize(size);

  for (size_t i = 0; i < size; ++i) {
    serializer(vector[i], "");
  }

  serializer.endArray();
  return true;
}

}

namespace crypto {

bool serialize(public_key& pubKey, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(pubKey, name, serializer);
}

bool serialize(secret_key& secKey, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(secKey, name, serializer);
}

bool serialize(hash& h, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(h, name, serializer);
}

bool serialize(key_image& keyImage, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(keyImage, name, serializer);
}

bool serialize(chacha8_iv& chacha, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(chacha, name, serializer);
}

bool serialize(signature& sig, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(sig, name, serializer);
}


}

namespace CryptoNote {

void serialize(TransactionPrefix& txP, ISerializer& serializer) {
  serializer(txP.version, "version");
  serializer(txP.unlockTime, "unlock_time");
  serializer(txP.vin, "vin");
  serializer(txP.vout, "vout");
  serializeAsBinary(txP.extra, "extra", serializer);
}

void serialize(Transaction& tx, ISerializer& serializer) {
  serializer(tx.version, "version");
  serializer(tx.unlockTime, "unlock_time");
  serializer(tx.vin, "vin");
  serializer(tx.vout, "vout");
  serializeAsBinary(tx.extra, "extra", serializer);

  std::size_t sigSize = tx.vin.size();
  //TODO: make arrays without sizes
//  serializer.beginArray(sigSize, "signatures");
  tx.signatures.resize(sigSize);

  bool signaturesNotExpected = tx.signatures.empty();
  if (!signaturesNotExpected && tx.vin.size() != tx.signatures.size()) {
    throw std::runtime_error("Serialization error: unexpected signatures size");
  }

  for (size_t i = 0; i < tx.vin.size(); ++i) {
    size_t signatureSize = Transaction::getSignatureSize(tx.vin[i]);
    if (signaturesNotExpected) {
      if (signatureSize == 0) {
        continue;
      } else {
        throw std::runtime_error("Serialization error: signatures are not expected");
      }
    }

    if (serializer.type() == ISerializer::OUTPUT) {
      if (signatureSize != tx.signatures[i].size()) {
        throw std::runtime_error("Serialization error: unexpected signatures size");
      }
    } else {
      tx.signatures[i].resize(signatureSize);
    }

    for (crypto::signature& sig: tx.signatures[i]) {
      serializePod(sig, "", serializer);
    }
  }
//  serializer.endArray();
}

void serialize(TransactionInput& in, ISerializer& serializer) {
  if (serializer.type() == ISerializer::OUTPUT) {
    BinaryVariantTagGetter tagGetter;
    uint8_t tag = boost::apply_visitor(tagGetter, in);
    serializer.binary(&tag, sizeof(tag), "type");

    VariantSerializer visitor(serializer, "value");
    boost::apply_visitor(visitor, in);
  } else {
    uint8_t tag;
    serializer.binary(&tag, sizeof(tag), "type");

    getVariantValue(serializer, tag, in);
  }
}

void serialize(TransactionInputGenerate& gen, ISerializer& serializer) {
  serializer(gen.height, "height");
}

void serialize(TransactionInputToScript& script, ISerializer& serializer) {}
void serialize(TransactionInputToScriptHash& scripthash, ISerializer& serializer) {}

void serialize(TransactionInputToKey& key, ISerializer& serializer) {
  serializer(key.amount, "amount");
  serializeVarintVector(key.keyOffsets, serializer, "key_offsets");
  serializer(key.keyImage, "k_image");
}

void serialize(TransactionInputMultisignature& multisignature, ISerializer& serializer) {
  serializer(multisignature.amount, "amount");
  serializer(multisignature.signatures, "signatures");
  serializer(multisignature.outputIndex, "outputIndex");
}

void serialize(TransactionOutput& output, ISerializer& serializer) {
  serializer(output.amount, "amount");
  serializer(output.target, "target");
}

void serialize(TransactionOutputTarget& output, ISerializer& serializer) {
  if (serializer.type() == ISerializer::OUTPUT) {
    BinaryVariantTagGetter tagGetter;
    uint8_t tag = boost::apply_visitor(tagGetter, output);
    serializer.binary(&tag, sizeof(tag), "type");

    VariantSerializer visitor(serializer, "data");
    boost::apply_visitor(visitor, output);
  } else {
    uint8_t tag;
    serializer.binary(&tag, sizeof(tag), "type");

    getVariantValue(serializer, tag, output);
  }
}

void serialize(TransactionOutputToScript& script, ISerializer& serializer) {}
void serialize(TransactionOutputToScriptHash& scripthash, ISerializer& serializer) {}

void serialize(TransactionOutputToKey& key, ISerializer& serializer) {
  serializer(key.key, "key");
}

void serialize(TransactionOutputMultisignature& multisignature, ISerializer& serializer) {
  serializer(multisignature.keys, "keys");
  serializer(multisignature.requiredSignatures, "required_signatures");
}

void serialize(ParentBlockSerializer& pbs, ISerializer& serializer) {
  serializer(pbs.m_parentBlock.majorVersion, "majorVersion");

  if (BLOCK_MAJOR_VERSION_1 < pbs.m_parentBlock.majorVersion) {
    throw std::runtime_error("Wrong parent block major version");
  }

  serializer(pbs.m_parentBlock.minorVersion, "minorVersion");
  serializer(pbs.m_timestamp, "timestamp");
  serializer(pbs.m_parentBlock.prevId, "prevId");
  serializer.binary(&pbs.m_nonce, sizeof(pbs.m_nonce), "nonce");

  if (pbs.m_hashingSerialization) {
    crypto::hash minerTxHash;
    if (!get_transaction_hash(pbs.m_parentBlock.minerTx, minerTxHash)) {
      throw std::runtime_error("Get transaction hash error");
    }

    crypto::hash merkleRoot;
    crypto::tree_hash_from_branch(pbs.m_parentBlock.minerTxBranch.data(), pbs.m_parentBlock.minerTxBranch.size(), minerTxHash, 0, merkleRoot);

    serializer(merkleRoot, "merkleRoot");
  }

  uint64_t txNum = static_cast<uint64_t>(pbs.m_parentBlock.numberOfTransactions);
  serializer(txNum, "numberOfTransactions");
  pbs.m_parentBlock.numberOfTransactions = static_cast<uint16_t>(txNum);
  if (pbs.m_parentBlock.numberOfTransactions < 1) {
    throw std::runtime_error("Wrong transactions number");
  }

  if (pbs.m_headerOnly) {
    return;
  }

  size_t branchSize = crypto::tree_depth(pbs.m_parentBlock.numberOfTransactions);
  if (serializer.type() == ISerializer::OUTPUT) {
    if (pbs.m_parentBlock.minerTxBranch.size() != branchSize) {
      throw std::runtime_error("Wrong miner transaction branch size");
    }
  } else {
    pbs.m_parentBlock.minerTxBranch.resize(branchSize);
  }

//  serializer(m_parentBlock.minerTxBranch, "minerTxBranch");
  //TODO: Make arrays with computable size! This code won't work with json serialization!
  for (crypto::hash& hash: pbs.m_parentBlock.minerTxBranch) {
    serializer(hash, "");
  }

  serializer(pbs.m_parentBlock.minerTx, "minerTx");

  tx_extra_merge_mining_tag mmTag;
  if (!get_mm_tag_from_extra(pbs.m_parentBlock.minerTx.extra, mmTag)) {
    throw std::runtime_error("Can't get extra merge mining tag");
  }

  if (mmTag.depth > 8 * sizeof(crypto::hash)) {
    throw std::runtime_error("Wrong merge mining tag depth");
  }

  if (serializer.type() == ISerializer::OUTPUT) {
    if (mmTag.depth != pbs.m_parentBlock.blockchainBranch.size()) {
      throw std::runtime_error("Blockchain branch size must be equal to merge mining tag depth");
    }
  } else {
    pbs.m_parentBlock.blockchainBranch.resize(mmTag.depth);
  }

//  serializer(m_parentBlock.blockchainBranch, "blockchainBranch");
  //TODO: Make arrays with computable size! This code won't work with json serialization!
  for (crypto::hash& hash: pbs.m_parentBlock.blockchainBranch) {
    serializer(hash, "");
  }
}

void serializeBlockHeader(BlockHeader& header, ISerializer& serializer) {
  serializer(header.majorVersion, "major_version");
  if (header.majorVersion > BLOCK_MAJOR_VERSION_2) {
    throw std::runtime_error("Wrong major version");
  }

  serializer(header.minorVersion, "minor_version");
  if (header.majorVersion == BLOCK_MAJOR_VERSION_1) {
    serializer(header.timestamp, "timestamp");
    serializer(header.prevId, "prev_id");
    serializer.binary(&header.nonce, sizeof(header.nonce), "nonce");
  } else if (header.majorVersion == BLOCK_MAJOR_VERSION_2) {
    serializer(header.prevId, "prev_id");
  } else {
    throw std::runtime_error("Wrong major version");
  }
}

void serialize(BlockHeader& header, ISerializer& serializer) {
  serializeBlockHeader(header, serializer);
}

void serialize(Block& block, ISerializer& serializer) {
  serializeBlockHeader(block, serializer);

  if (block.majorVersion == BLOCK_MAJOR_VERSION_2) {
    auto parentBlockSerializer = makeParentBlockSerializer(block, false, false);
    serializer(parentBlockSerializer, "parent_block");
  }

  serializer(block.minerTx, "miner_tx");
  serializer(block.txHashes, "tx_hashes");
}

void serialize(AccountPublicAddress& address, ISerializer& serializer) {
  serializer(address.m_spendPublicKey, "m_spend_public_key");
  serializer(address.m_viewPublicKey, "m_view_public_key");
}

void serialize(account_keys& keys, ISerializer& s) {
  s(keys.m_account_address, "m_account_address");
  s(keys.m_spend_secret_key, "m_spend_secret_key");
  s(keys.m_view_secret_key, "m_view_secret_key");
}

void doSerialize(tx_extra_merge_mining_tag& tag, ISerializer& serializer) {
  uint64_t depth = static_cast<uint64_t>(tag.depth);
  serializer(depth, "depth");
  tag.depth = static_cast<size_t>(depth);
  serializer(tag.merkle_root, "merkle_root");
}

void serialize(tx_extra_merge_mining_tag& tag, ISerializer& serializer) {
  if (serializer.type() == ISerializer::OUTPUT) {
    std::stringstream stream;
    BinaryOutputStreamSerializer output(stream);
    doSerialize(tag, output);
    std::string field = stream.str();
    serializer(field, "");
  } else {
    std::string field;
    serializer(field, "");
    std::stringstream stream(field);
    BinaryInputStreamSerializer input(stream);
    doSerialize(tag, input);
  }
}

} //namespace CryptoNote
