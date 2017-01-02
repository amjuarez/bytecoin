// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CryptoNoteSerialization.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"

#include "Common/StringOutputStream.h"
#include "crypto/crypto.h"

#include "Account.h"
#include "CryptoNoteConfig.h"
#include "CryptoNoteFormatUtils.h"
#include "CryptoNoteTools.h"
#include "TransactionExtra.h"

using namespace Common;

namespace {

using namespace CryptoNote;
using namespace Common;

size_t getSignaturesCount(const TransactionInput& input) {
  struct txin_signature_size_visitor : public boost::static_visitor < size_t > {
    size_t operator()(const BaseInput& txin) const { return 0; }
    size_t operator()(const KeyInput& txin) const { return txin.outputIndexes.size(); }
    size_t operator()(const MultisignatureInput& txin) const { return txin.signatureCount; }
  };

  return boost::apply_visitor(txin_signature_size_visitor(), input);
}

struct BinaryVariantTagGetter: boost::static_visitor<uint8_t> {
  uint8_t operator()(const CryptoNote::BaseInput) { return  0xff; }
  uint8_t operator()(const CryptoNote::KeyInput) { return  0x2; }
  uint8_t operator()(const CryptoNote::MultisignatureInput) { return  0x3; }
  uint8_t operator()(const CryptoNote::KeyOutput) { return  0x2; }
  uint8_t operator()(const CryptoNote::MultisignatureOutput) { return  0x3; }
  uint8_t operator()(const CryptoNote::Transaction) { return  0xcc; }
  uint8_t operator()(const CryptoNote::Block) { return  0xbb; }
};

struct VariantSerializer : boost::static_visitor<> {
  VariantSerializer(CryptoNote::ISerializer& serializer, const std::string& name) : s(serializer), name(name) {}

  template <typename T>
  void operator() (T& param) { s(param, name); }

  CryptoNote::ISerializer& s;
  std::string name;
};

void getVariantValue(CryptoNote::ISerializer& serializer, uint8_t tag, CryptoNote::TransactionInput& in) {
  switch(tag) {
  case 0xff: {
    CryptoNote::BaseInput v;
    serializer(v, "value");
    in = v;
    break;
  }
  case 0x2: {
    CryptoNote::KeyInput v;
    serializer(v, "value");
    in = v;
    break;
  }
  case 0x3: {
    CryptoNote::MultisignatureInput v;
    serializer(v, "value");
    in = v;
    break;
  }
  default:
    throw std::runtime_error("Unknown variant tag");
  }
}

void getVariantValue(CryptoNote::ISerializer& serializer, uint8_t tag, CryptoNote::TransactionOutputTarget& out) {
  switch(tag) {
  case 0x2: {
    CryptoNote::KeyOutput v;
    serializer(v, "data");
    out = v;
    break;
  }
  case 0x3: {
    CryptoNote::MultisignatureOutput v;
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

bool serializeVarintVector(std::vector<uint32_t>& vector, CryptoNote::ISerializer& serializer, Common::StringView name) {
  size_t size = vector.size();
  
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

namespace Crypto {

bool serialize(PublicKey& pubKey, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(pubKey, name, serializer);
}

bool serialize(SecretKey& secKey, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(secKey, name, serializer);
}

bool serialize(Hash& h, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(h, name, serializer);
}

bool serialize(KeyImage& keyImage, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(keyImage, name, serializer);
}

bool serialize(chacha_iv& chacha, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(chacha, name, serializer);
}

bool serialize(Signature& sig, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(sig, name, serializer);
}

bool serialize(EllipticCurveScalar& ecScalar, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(ecScalar, name, serializer);
}

bool serialize(EllipticCurvePoint& ecPoint, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(ecPoint, name, serializer);
}

}

namespace rct {

bool serialize(key& aKey, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(aKey, name, serializer);
}

bool serialize(key64& aKey64, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(aKey64, name, serializer);
}

bool serialize(ctkey& aCtkey, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(aCtkey, name, serializer);
}

bool serialize(boroSig& aBoroSig, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializePod(aBoroSig, name, serializer);
}

bool serialize(ecdhTuple& anEcdhTuple, Common::StringView name, CryptoNote::ISerializer& serializer) {
  serializer(anEcdhTuple.mask, "mask");
  serializer(anEcdhTuple.amount, "amount");
  return true;
}

bool serialize(mgSig& aMgSig, Common::StringView name, CryptoNote::ISerializer& serializer) {
  serializer(aMgSig.ss, "ss");
  serializer(aMgSig.cc, "cc");
  return true;
}

bool serialize(rangeSig& aRangeSig, Common::StringView name, CryptoNote::ISerializer& serializer) {
  serializer(aRangeSig.asig, "asig");
  serializer(aRangeSig.Ci, "Ci");
  return true;
}

}

namespace CryptoNote {

void serializeRctsigBase(rct::rctSigBase& rctSigBase, size_t inputs, size_t outputs, ISerializer& serializer) {
  serializer(rctSigBase.type, "type");
  if (rctSigBase.type == rct::RCTTypeNull) {
    return;
  }

  if (rctSigBase.type != rct::RCTTypeFull && rctSigBase.type != rct::RCTTypeSimple) {
    throw std::runtime_error("Wrong rctSigBase type: " + std::to_string(static_cast<int>(rctSigBase.type)));
  }

  serializer(rctSigBase.txnFee, "txnFee");

  // message - not serialized, it can be reconstructed
  // mixRing - not serialized, it can be reconstructed
  if (rctSigBase.type == rct::RCTTypeSimple) {
    if (serializer.type() == ISerializer::INPUT) {
      rctSigBase.pseudoOuts.resize(inputs);
    } else if (rctSigBase.pseudoOuts.size() != inputs) {
      throw std::runtime_error("rctSigBase.pseudoOuts.size() != inputs");
    }

    for (size_t i = 0; i < inputs; ++i) {
      serializer(rctSigBase.pseudoOuts[i], "");
    }
  }

  if (serializer.type() == ISerializer::INPUT) {
    rctSigBase.ecdhInfo.resize(outputs);
  } else if (rctSigBase.ecdhInfo.size() != outputs) {
    throw std::runtime_error("rctSigBase.ecdhInfo.size() != outputs");
  }

  for (size_t i = 0; i < outputs; ++i) {
    serializer(rctSigBase.ecdhInfo[i], "");
  }

  if (serializer.type() == ISerializer::INPUT) {
    rctSigBase.outPk.resize(outputs);
  } else if (rctSigBase.outPk.size() != outputs) {
    throw std::runtime_error("rctSigBase.outPk.size() != outputs");
  }

  for (size_t i = 0; i < outputs; ++i) {
    serializer(rctSigBase.outPk[i].mask, "");
  }
}

void serializeRctSigPrunable(rct::rctSigPrunable& rctSigPrunable, uint8_t type, size_t inputs, size_t outputs, size_t mixin, ISerializer& serializer) {
  if (type == rct::RCTTypeNull) {
    return;
  }

  if (type != rct::RCTTypeFull && type != rct::RCTTypeSimple) {
    throw std::runtime_error("Wrong type : " + std::to_string(static_cast<int>(type)));
  }

  if (serializer.type() == ISerializer::INPUT) {
    rctSigPrunable.rangeSigs.resize(outputs);
  } else if (rctSigPrunable.rangeSigs.size() != outputs) {
    throw std::runtime_error("rctSigPrunable.rangeSigs.size() != outputs");
  }

  for (size_t i = 0; i < outputs; ++i) {
    serializer(rctSigPrunable.rangeSigs[i], "");
  }

  // we keep a byte for size of MGs, because we don't know whether this is
  // a simple or full rct signature, and it's starting to annoy the hell out of me
  size_t mgElements = type == rct::RCTTypeSimple ? inputs : 1;

  if (serializer.type() == ISerializer::INPUT) {
    rctSigPrunable.MGs.resize(outputs);
  } else if (rctSigPrunable.MGs.size() != mgElements) {
    throw std::runtime_error("rctSigPrunable.MGs.size() != mgElements");
  }

  for (size_t i = 0; i < mgElements; ++i) {
    // we save the MGs contents directly, because we want it to save its
    // arrays and matrices without the size prefixes, and the load can't
    // know what size to expect if it's not in the data
    if (serializer.type() == ISerializer::INPUT) {
      rctSigPrunable.MGs[i].ss.resize(mixin + 1);
    } else if (rctSigPrunable.MGs[i].ss.size() != mixin + 1) {
      throw std::runtime_error("rctSigPrunable.MGs[i].ss.size() != mixin + 1");
    }

    for (size_t j = 0; j < mixin + 1; ++j) {
      size_t mgSs2Elements = (type == rct::RCTTypeSimple ? 1 : inputs) + 1;
      if (serializer.type() == ISerializer::INPUT) {
        rctSigPrunable.MGs[i].ss[j].resize(mgSs2Elements);
      } else if (rctSigPrunable.MGs[i].ss[j].size() != mgSs2Elements) {
        throw std::runtime_error("rctSigPrunable.MGs[i].ss[j].size() != mgSs2Elements");
      }

      for (size_t k = 0; k < mgSs2Elements; ++k) {
        serializer(rctSigPrunable.MGs[i].ss[j][k], "");
      }
    }

    // MGs[i].II not saved, it can be reconstructed
    serializer(rctSigPrunable.MGs[i].cc, "");
  }
}

void serialize(TransactionPrefix& txP, ISerializer& serializer) {
  serializer(txP.version, "version");
  serializer(txP.unlockTime, "unlock_time");
  serializer(txP.inputs, "vin");
  serializer(txP.outputs, "vout");
  serializeAsBinary(txP.extra, "extra", serializer);
}

void serialize(Transaction& tx, ISerializer& serializer) {
  serialize(static_cast<TransactionPrefix&>(tx), serializer);

  if (TRANSACTION_VERSION_2 < tx.version) {
    throw std::runtime_error("Wrong transaction version");
  }

  size_t sigSize = tx.inputs.size();
  
  if (serializer.type() == ISerializer::INPUT) {
    tx.signatures.resize(sigSize);
  }

  bool signaturesNotExpected = tx.signatures.empty();
  if (!signaturesNotExpected && tx.inputs.size() != tx.signatures.size()) {
    throw std::runtime_error("Serialization error: unexpected signatures size");
  }

  for (size_t i = 0; i < tx.inputs.size(); ++i) {
    size_t signatureSize = getSignaturesCount(tx.inputs[i]);
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

      for (Crypto::Signature& sig : tx.signatures[i]) {
        serializePod(sig, "", serializer);
      }

    } else {
      std::vector<Crypto::Signature> signatures(signatureSize);
      for (Crypto::Signature& sig : signatures) {
        serializePod(sig, "", serializer);
      }

      tx.signatures[i] = std::move(signatures);
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

void serialize(BaseInput& gen, ISerializer& serializer) {
  serializer(gen.blockIndex, "height");
}

void serialize(KeyInput& key, ISerializer& serializer) {
  serializer(key.amount, "amount");
  serializeVarintVector(key.outputIndexes, serializer, "key_offsets");
  serializer(key.keyImage, "k_image");
}

void serialize(MultisignatureInput& multisignature, ISerializer& serializer) {
  serializer(multisignature.amount, "amount");
  serializer(multisignature.signatureCount, "signatures");
  serializer(multisignature.outputIndex, "outputIndex");
  serializer(multisignature.term, "term");
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

void serialize(KeyOutput& key, ISerializer& serializer) {
  serializer(key.key, "key");
}

void serialize(MultisignatureOutput& multisignature, ISerializer& serializer) {
  serializer(multisignature.keys, "keys");
  serializer(multisignature.requiredSignatureCount, "required_signatures");
  serializer(multisignature.term, "term");
}

void serialize(RootBlockTransaction& tx, ISerializer& serializer) {
  serialize(static_cast<TransactionPrefix&>(tx), serializer);

  if (tx.version < TRANSACTION_VERSION_2) {
    size_t sigSize = tx.inputs.size();
    //TODO: make arrays without sizes
//  serializer.beginArray(sigSize, "signatures");

    if (serializer.type() == ISerializer::INPUT) {
      tx.signatures.resize(sigSize);
    }

    bool signaturesNotExpected = tx.signatures.empty();
    if (!signaturesNotExpected && tx.inputs.size() != tx.signatures.size()) {
      throw std::runtime_error("Serialization error: unexpected signatures size");
    }

    for (size_t i = 0; i < tx.inputs.size(); ++i) {
      size_t signatureSize = getSignaturesCount(tx.inputs[i]);
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

        for (Crypto::Signature& sig : tx.signatures[i]) {
          serializePod(sig, "", serializer);
        }

      } else {
        std::vector<Crypto::Signature> signatures(signatureSize);
        for (Crypto::Signature& sig : signatures) {
          serializePod(sig, "", serializer);
        }

        tx.signatures[i] = std::move(signatures);
      }
    }
//  serializer.endArray();
  } else {
    if (!tx.inputs.empty()) {
      serializeRctsigBase(tx.rctSignatures, tx.inputs.size(), tx.outputs.size(), serializer);

      if (tx.rctSignatures.type != rct::RCTTypeNull) {
        serializeRctSigPrunable(tx.rctSignatures.p, tx.rctSignatures.type, tx.inputs.size(), tx.outputs.size(),
                                tx.inputs[0].type() == typeid(KeyInput) ? boost::get<KeyInput>(tx.inputs[0]).outputIndexes.size() - 1 : 0, serializer);
      }
    }
  }
}

bool getRootBlockTransactionHash(const RootBlockTransaction& tx, Crypto::Hash& hash) {
  if (tx.version < TRANSACTION_VERSION_2) {
    return getObjectHash(tx, hash);
  } else {
    Crypto::Hash hashes[3];

    // prefix
    hashes[0] = getObjectHash(static_cast<const TransactionPrefix&>(tx));

    RootBlockTransaction& txRef = const_cast<RootBlockTransaction&>(tx);

    // base rct
    try {
      BinaryArray binaryArray;
      ::Common::VectorOutputStream stream(binaryArray);
      BinaryOutputStreamSerializer serializer(stream);
      serializeRctsigBase(txRef.rctSignatures, tx.inputs.size(), tx.outputs.size(), serializer);

      hashes[1] = getBinaryArrayHash(binaryArray);
    } catch (std::exception&) {
      return false;
    }

    // prunable rct
    if (tx.rctSignatures.type == rct::RCTTypeNull) {
      hashes[2] = CryptoNote::NULL_HASH;
    } else {
      try {
        BinaryArray binaryArray;
        ::Common::VectorOutputStream stream(binaryArray);
        BinaryOutputStreamSerializer serializer(stream);
        size_t mixin = tx.inputs.empty() ? 0 : tx.inputs[0].type() == typeid(KeyInput) ? boost::get<KeyInput>(tx.inputs[0]).outputIndexes.size() - 1 : 0;
        serializeRctSigPrunable(txRef.rctSignatures.p, tx.rctSignatures.type, tx.inputs.size(), tx.outputs.size(), mixin, serializer);

        hashes[2] = getBinaryArrayHash(binaryArray);
      } catch (std::exception&) {
        return false;
      }
    }

    // the tx hash is the hash of the 3 hashes
    hash = cn_fast_hash(hashes, sizeof(hashes));

    return true;
  }

}

void serialize(RootBlockSerializer& pbs, ISerializer& serializer) {
  serializer(pbs.m_rootBlock.majorVersion, "majorVersion");
  serializer(pbs.m_rootBlock.minorVersion, "minorVersion");
  serializer(pbs.m_timestamp, "timestamp");
  serializer(pbs.m_rootBlock.previousBlockHash, "prevId");
  serializer.binary(&pbs.m_nonce, sizeof(pbs.m_nonce), "nonce");

  if (pbs.m_hashingSerialization) {
    Crypto::Hash minerTxHash;
    if (!getRootBlockTransactionHash(pbs.m_rootBlock.baseTransaction, minerTxHash)) {
      throw std::runtime_error("Get transaction hash error");
    }

    Crypto::Hash merkleRoot;
    Crypto::tree_hash_from_branch(pbs.m_rootBlock.baseTransactionBranch.data(), pbs.m_rootBlock.baseTransactionBranch.size(), minerTxHash, 0, merkleRoot);

    serializer(merkleRoot, "merkleRoot");
  }

  uint64_t txNum = static_cast<uint64_t>(pbs.m_rootBlock.transactionCount);
  serializer(txNum, "numberOfTransactions");
  pbs.m_rootBlock.transactionCount = static_cast<uint16_t>(txNum);
  if (pbs.m_rootBlock.transactionCount < 1) {
    throw std::runtime_error("Wrong transactions number");
  }

  if (pbs.m_headerOnly) {
    return;
  }

  size_t branchSize = Crypto::tree_depth(pbs.m_rootBlock.transactionCount);
  if (serializer.type() == ISerializer::OUTPUT) {
    if (pbs.m_rootBlock.baseTransactionBranch.size() != branchSize) {
      throw std::runtime_error("Wrong miner transaction branch size");
    }
  } else {
    pbs.m_rootBlock.baseTransactionBranch.resize(branchSize);
  }

  for (Crypto::Hash& hash: pbs.m_rootBlock.baseTransactionBranch) {
    serializer(hash, "");
  }

  serializer(pbs.m_rootBlock.baseTransaction, "minerTx");

  TransactionExtraMergeMiningTag mmTag;
  if (!getMergeMiningTagFromExtra(pbs.m_rootBlock.baseTransaction.extra, mmTag)) {
    throw std::runtime_error("Can't get extra merge mining tag");
  }

  if (mmTag.depth > 8 * sizeof(Crypto::Hash)) {
    throw std::runtime_error("Wrong merge mining tag depth");
  }

  if (serializer.type() == ISerializer::OUTPUT) {
    if (mmTag.depth != pbs.m_rootBlock.blockchainBranch.size()) {
      throw std::runtime_error("Blockchain branch size must be equal to merge mining tag depth");
    }
  } else {
    pbs.m_rootBlock.blockchainBranch.resize(mmTag.depth);
  }

  for (Crypto::Hash& hash: pbs.m_rootBlock.blockchainBranch) {
    serializer(hash, "");
  }
}

void serializeBlockHeader(BlockHeader& header, ISerializer& serializer) {
  serializer(header.majorVersion, "major_version");
  if (header.majorVersion > BLOCK_MAJOR_VERSION_4) {
    throw std::runtime_error("Wrong major version");
  }

  serializer(header.minorVersion, "minor_version");
  if (header.majorVersion < BLOCK_MAJOR_VERSION_3) {
    serializer(header.timestamp, "timestamp");
    serializer(header.previousBlockHash, "prev_id");
    serializer.binary(&header.nonce, sizeof(header.nonce), "nonce");
  } else {
    serializer(header.previousBlockHash, "prev_id");
  }
}

void serialize(BlockHeader& header, ISerializer& serializer) {
  serializeBlockHeader(header, serializer);
}

void serialize(Block& block, ISerializer& serializer) {
  serializeBlockHeader(block, serializer);

  if (block.majorVersion >= BLOCK_MAJOR_VERSION_3) {
    auto rootBlockSerializer = makeRootBlockSerializer(block, false, false);
    serializer(rootBlockSerializer, "root_block");
  }

  serializer(block.baseTransaction, "miner_tx");
  serializer(block.transactionHashes, "tx_hashes");
}

void serialize(AccountPublicAddress& address, ISerializer& serializer) {
  serializer(address.spendPublicKey, "m_spend_public_key");
  serializer(address.viewPublicKey, "m_view_public_key");
}

void serialize(AccountKeys& keys, ISerializer& s) {
  s(keys.address, "m_account_address");
  s(keys.spendSecretKey, "m_spend_secret_key");
  s(keys.viewSecretKey, "m_view_secret_key");
}

void doSerialize(TransactionExtraMergeMiningTag& tag, ISerializer& serializer) {
  uint64_t depth = static_cast<uint64_t>(tag.depth);
  serializer(depth, "depth");
  tag.depth = static_cast<size_t>(depth);
  serializer(tag.merkleRoot, "merkle_root");
}

void serialize(TransactionExtraMergeMiningTag& tag, ISerializer& serializer) {
  if (serializer.type() == ISerializer::OUTPUT) {
    std::string field;
    StringOutputStream os(field);
    BinaryOutputStreamSerializer output(os);
    doSerialize(tag, output);
    serializer(field, "");
  } else {
    std::string field;
    serializer(field, "");
    MemoryInputStream stream(field.data(), field.size());
    BinaryInputStreamSerializer input(stream);
    doSerialize(tag, input);
  }
}

void serialize(KeyPair& keyPair, ISerializer& serializer) {
  serializer(keyPair.secretKey, "secret_key");
  serializer(keyPair.publicKey, "public_key");
}


} //namespace CryptoNote
