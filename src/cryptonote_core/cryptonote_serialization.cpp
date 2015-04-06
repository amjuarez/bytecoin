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

#include "cryptonote_serialization.h"

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
  uint8_t operator()(const cryptonote::TransactionInputGenerate) { return  0xff; }
  uint8_t operator()(const cryptonote::TransactionInputToScript) { return  0x0; }
  uint8_t operator()(const cryptonote::TransactionInputToScriptHash) { return  0x1; }
  uint8_t operator()(const cryptonote::TransactionInputToKey) { return  0x2; }
  uint8_t operator()(const cryptonote::TransactionInputMultisignature) { return  0x3; }
  uint8_t operator()(const cryptonote::TransactionOutputToScript) { return  0x0; }
  uint8_t operator()(const cryptonote::TransactionOutputToScriptHash) { return  0x1; }
  uint8_t operator()(const cryptonote::TransactionOutputToKey) { return  0x2; }
  uint8_t operator()(const cryptonote::TransactionOutputMultisignature) { return  0x3; }
  uint8_t operator()(const cryptonote::Transaction) { return  0xcc; }
  uint8_t operator()(const cryptonote::Block) { return  0xbb; }
};

struct VariantSerializer : boost::static_visitor<> {
  VariantSerializer(cryptonote::ISerializer& serializer, const std::string& name) : s(serializer), name(name) {}

  void operator() (cryptonote::TransactionInputGenerate& param) { s(param, name); }
  void operator() (cryptonote::TransactionInputToScript& param) { s(param, name); }
  void operator() (cryptonote::TransactionInputToScriptHash& param) { s(param, name); }
  void operator() (cryptonote::TransactionInputToKey& param) { s(param, name); }
  void operator() (cryptonote::TransactionInputMultisignature& param) { s(param, name); }
  void operator() (cryptonote::TransactionOutputToScript& param) { s(param, name); }
  void operator() (cryptonote::TransactionOutputToScriptHash& param) { s(param, name); }
  void operator() (cryptonote::TransactionOutputToKey& param) { s(param, name); }
  void operator() (cryptonote::TransactionOutputMultisignature& param) { s(param, name); }

  cryptonote::ISerializer& s;
  const std::string& name;
};

void getVariantValue(cryptonote::ISerializer& serializer, uint8_t tag, cryptonote::TransactionInput& in) {
  switch(tag) {
  case 0xff: {
    cryptonote::TransactionInputGenerate v;
    serializer(v, "data");
    in = v;
    break;
  }
  case 0x0: {
    cryptonote::TransactionInputToScript v;
    serializer(v, "data");
    in = v;
    break;
  }
  case 0x1: {
    cryptonote::TransactionInputToScriptHash v;
    serializer(v, "data");
    in = v;
    break;
  }
  case 0x2: {
    cryptonote::TransactionInputToKey v;
    serializer(v, "data");
    in = v;
    break;
  }
  case 0x3: {
    cryptonote::TransactionInputMultisignature v;
    serializer(v, "data");
    in = v;
    break;
  }
  default:
    throw std::runtime_error("Unknown variant tag");
  }
}

void getVariantValue(cryptonote::ISerializer& serializer, uint8_t tag, cryptonote::TransactionOutputTarget& out) {
  switch(tag) {
  case 0x0: {
    cryptonote::TransactionOutputToScript v;
    serializer(v, "data");
    out = v;
    break;
  }
  case 0x1: {
    cryptonote::TransactionOutputToScriptHash v;
    serializer(v, "data");
    out = v;
    break;
  }
  case 0x2: {
    cryptonote::TransactionOutputToKey v;
    serializer(v, "data");
    out = v;
    break;
  }
  case 0x3: {
    cryptonote::TransactionOutputMultisignature v;
    serializer(v, "data");
    out = v;
    break;
  }
  default:
    throw std::runtime_error("Unknown variant tag");
  }
}

template <typename T>
void serializePod(T& v, const std::string& name, cryptonote::ISerializer& serializer) {
  serializer.binary(&v, sizeof(v), name);
}

void serializeVarintVector(std::vector<uint64_t>& vector, cryptonote::ISerializer& serializer, const std::string& name) {
  std::size_t size = vector.size();
  serializer.beginArray(size, name);
  vector.resize(size);

  for (size_t i = 0; i < size; ++i) {
    serializer(vector[i], "");
  }

  serializer.endArray();
}

}

namespace crypto {

void serialize(public_key& pubKey, const std::string& name, cryptonote::ISerializer& serializer) {
  serializePod(pubKey, name, serializer);
}

void serialize(secret_key& secKey, const std::string& name, cryptonote::ISerializer& serializer) {
  serializePod(secKey, name, serializer);
}

void serialize(hash& h, const std::string& name, cryptonote::ISerializer& serializer) {
  serializePod(h, name, serializer);
}

void serialize(key_image& keyImage, const std::string& name, cryptonote::ISerializer& serializer) {
  serializePod(keyImage, name, serializer);
}

void serialize(chacha8_iv& chacha, const std::string& name, cryptonote::ISerializer& serializer) {
  serializePod(chacha, name, serializer);
}

}

namespace cryptonote {

void serialize(TransactionPrefix& txP, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  uint64_t version = static_cast<uint64_t>(txP.version);
  serializer(version, "version");
  txP.version = static_cast<size_t>(version);
  serializer(txP.unlockTime, "unlock_time");
  serializer(txP.vin, "vin");
  serializer(txP.vout, "vout");
  serializeAsBinary(txP.extra, "extra", serializer);
  serializer.endObject();
}

void serialize(Transaction& tx, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);

  uint64_t version = static_cast<uint64_t>(tx.version);
  serializer(version, "version");
  tx.version = static_cast<size_t>(version);
  //TODO: make version. check version here
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

  serializer.endObject();
}

void serialize(TransactionInput& in, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);

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

  serializer.endObject();
}

void serialize(TransactionInputGenerate& gen, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  uint64_t height = static_cast<uint64_t>(gen.height);
  serializer(height, "height");
  gen.height = static_cast<size_t>(height);
  serializer.endObject();
}

void serialize(TransactionInputToScript& script, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializer.endObject();
}

void serialize(TransactionInputToScriptHash& scripthash, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializer.endObject();
}

void serialize(TransactionInputToKey& key, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(key.amount, "amount");
  serializeVarintVector(key.keyOffsets, serializer, "key_offsets");
  serializer(key.keyImage, "k_image");
  serializer.endObject();
}

void serialize(TransactionInputMultisignature& multisignature, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(multisignature.amount, "amount");
  serializer(multisignature.signatures, "signatures");
  serializer(multisignature.outputIndex, "outputIndex");
  serializer.endObject();
}

void serialize(TransactionOutput& output, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(output.amount, "amount");
  serializer(output.target, "target");
  serializer.endObject();
}

void serialize(TransactionOutputTarget& output, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);

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

  serializer.endObject();
}

void serialize(TransactionOutputToScript& script, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializer.endObject();
}

void serialize(TransactionOutputToScriptHash& scripthash, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializer.endObject();
}

void serialize(TransactionOutputToKey& key, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(key.key, "key");
  serializer.endObject();
}

void serialize(TransactionOutputMultisignature& multisignature, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(multisignature.keys, "keys");
  serializer(multisignature.requiredSignatures, "required_signatures");
  serializer.endObject();
}

void serialize(ParentBlockSerializer& pbs, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);

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
  pbs.m_parentBlock.numberOfTransactions = static_cast<size_t>(txNum);
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

  serializer.endObject();
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

void serialize(BlockHeader& header, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializeBlockHeader(header, serializer);
  serializer.endObject();
}

void serialize(Block& block, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);

  serializeBlockHeader(block, serializer);

  if (block.majorVersion == BLOCK_MAJOR_VERSION_2) {
    auto parentBlockSerializer = makeParentBlockSerializer(block, false, false);
    serializer(parentBlockSerializer, "parent_block");
  }

  serializer(block.minerTx, "miner_tx");
  serializer(block.txHashes, "tx_hashes");

  serializer.endObject();
}

void serialize(AccountPublicAddress& address, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);

  serializer(address.m_spendPublicKey, "spend_public_key");
  serializer(address.m_viewPublicKey, "view_public_key");

  serializer.endObject();
}

void doSerialize(tx_extra_merge_mining_tag& tag, const std::string& name, ISerializer& serializer) {
  uint64_t depth = static_cast<uint64_t>(tag.depth);
  serializer(depth, "depth");
  tag.depth = static_cast<size_t>(depth);
  serializer(tag.merkle_root, "merkle_root");
}

void serialize(tx_extra_merge_mining_tag& tag, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);

  if (serializer.type() == ISerializer::OUTPUT) {
    std::stringstream stream;
    BinaryOutputStreamSerializer output(stream);
    doSerialize(tag, "", output);
    std::string field = stream.str();
    serializer(field, "");
  } else {
    std::string field;
    serializer(field, "");

    std::stringstream stream(field);
    BinaryInputStreamSerializer input(stream);
    doSerialize(tag, "", input);
  }

  serializer.endObject();
}

} //namespace cryptonote
