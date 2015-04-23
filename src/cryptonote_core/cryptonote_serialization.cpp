// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

void serializeBlockHeader(BlockHeader& header, ISerializer& serializer) {
  serializer(header.majorVersion, "major_version");
  if (header.majorVersion > BLOCK_MAJOR_VERSION_1) {
    throw std::runtime_error("Wrong major version");
  }

  serializer(header.minorVersion, "minor_version");
  serializer(header.timestamp, "timestamp");
  serializer(header.prevId, "prev_id");
  serializer.binary(&header.nonce, sizeof(header.nonce), "nonce");
}

void serialize(BlockHeader& header, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);
  serializeBlockHeader(header, serializer);
  serializer.endObject();
}

void serialize(Block& block, const std::string& name, ISerializer& serializer) {
  serializer.beginObject(name);

  serializeBlockHeader(block, serializer);

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

} //namespace cryptonote
