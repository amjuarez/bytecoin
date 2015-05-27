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

#include "cryptonote_basic.h"
#include "../Common/StreamTools.h"
#include "../Common/StringTools.h"

using Common::IInputStream;
using Common::IOutputStream;
using Common::read;
using Common::readVarint;
using Common::toString;

namespace CryptoNote {

void TransactionInputGenerate::serialize(IOutputStream& out) const {
  writeVarint(out, height);
}

TransactionInputGenerate TransactionInputGenerate::deserialize(IInputStream& in) {
  TransactionInputGenerate input;
  readVarint(in, input.height);
  return input;
}

void TransactionInputToKey::serialize(IOutputStream& out) const {
  writeVarint(out, amount);
  writeVarint(out, keyOffsets.size());
  for (uint64_t outputIndex : keyOffsets) {
    writeVarint(out, outputIndex);
  }

  write(out, &keyImage, sizeof(keyImage));
}

TransactionInputToKey TransactionInputToKey::deserialize(IInputStream& in) {
  TransactionInputToKey input;
  readVarint(in, input.amount);
  input.keyOffsets.resize(readVarint<uint16_t>(in));
  for (uint64_t& outputIndex : input.keyOffsets) {
    readVarint(in, outputIndex);
  }

  read(in, &input.keyImage, sizeof(input.keyImage));
  return input;
}

void TransactionInputMultisignature::serialize(IOutputStream& out) const {
  writeVarint(out, amount);
  writeVarint(out, signatures);
  writeVarint(out, outputIndex);
}

TransactionInputMultisignature TransactionInputMultisignature::deserialize(IInputStream& in) {
  TransactionInputMultisignature input;
  readVarint(in, input.amount);
  readVarint(in, input.signatures);
  readVarint(in, input.outputIndex);
  return input;
}

void TransactionOutputToKey::serialize(IOutputStream& out) const {
  write(out, &key, sizeof(key));
}

TransactionOutputToKey TransactionOutputToKey::deserialize(IInputStream& in) {
  TransactionOutputToKey output;
  read(in, &output.key, sizeof(output.key));
  return output;
}

void TransactionOutputMultisignature::serialize(IOutputStream& out) const {
  writeVarint(out, keys.size());
  for (const crypto::public_key& key : keys) {
    write(out, &key, sizeof(key));
  }

  writeVarint(out, requiredSignatures);
}

TransactionOutputMultisignature TransactionOutputMultisignature::deserialize(IInputStream& in) {
  TransactionOutputMultisignature output;
  output.keys.resize(readVarint<uint16_t>(in));
  for (crypto::public_key& key : output.keys) {
    read(in, &key, sizeof(key));
  }

  readVarint(in, output.requiredSignatures);
  if (output.requiredSignatures > output.keys.size()) {
    throw std::runtime_error("TransactionOutputMultisignature::deserialize");
  }

  return output;
}

void TransactionOutput::serialize(IOutputStream& out) const {
  writeVarint(out, amount);
  if (target.type() == typeid(TransactionOutputToKey)) {
    write(out, static_cast<uint8_t>(2));
    boost::get<TransactionOutputToKey>(target).serialize(out);
  } else {
    write(out, static_cast<uint8_t>(3));
    boost::get<TransactionOutputMultisignature>(target).serialize(out);
  }
}

TransactionOutput TransactionOutput::deserialize(IInputStream& in) {
  TransactionOutput output;
  readVarint(in, output.amount);
  uint8_t targetType = read<uint8_t>(in);
  if (targetType == 2) {
    output.target = TransactionOutputToKey::deserialize(in);
  } else if (targetType == 3) {
    output.target = TransactionOutputMultisignature::deserialize(in);
  } else {
    throw std::runtime_error("TransactionOutput::deserialize");
  }

  return output;
}

void Transaction::serialize(IOutputStream& out) const {
  writeVarint(out, version);
  writeVarint(out, unlockTime);
  writeVarint(out, vin.size());
  for (const TransactionInput& input : vin) {
    if (input.type() == typeid(TransactionInputGenerate)) {
      write(out, static_cast<uint8_t>(255));
      boost::get<TransactionInputGenerate>(input).serialize(out);
    } else if (input.type() == typeid(TransactionInputToKey)) {
      write(out, static_cast<uint8_t>(2));
      boost::get<TransactionInputToKey>(input).serialize(out);
    } else {
      write(out, static_cast<uint8_t>(3));
      boost::get<TransactionInputMultisignature>(input).serialize(out);
    }
  }

  writeVarint(out, vout.size());
  for (const TransactionOutput& output : vout) {
    output.serialize(out);
  }

  writeVarint(out, extra.size());
  write(out, extra);
  std::size_t signatureCount = 0;
  for (const std::vector<crypto::signature>& inputSignatures : signatures) {
    signatureCount += inputSignatures.size();
  }

  for (const std::vector<crypto::signature>& inputSignatures : signatures) {
    for (const crypto::signature& signature : inputSignatures) {
      write(out, &signature, sizeof(signature));
    }
  }
}

Transaction Transaction::deserialize(IInputStream& in) {
  Transaction transaction;
  readVarint(in, transaction.version);
  if (transaction.version != CURRENT_TRANSACTION_VERSION) {
    throw std::runtime_error("Transaction::deserialize");
  }

  readVarint(in, transaction.unlockTime);
  transaction.vin.resize(readVarint<uint16_t>(in));
  for (TransactionInput& input : transaction.vin) {
    uint8_t inputType = read<uint8_t>(in);
    if (inputType == 255) {
      input = TransactionInputGenerate::deserialize(in);
    } else if (inputType == 2) {
      input = TransactionInputToKey::deserialize(in);
    } else if (inputType == 3) {
      input = TransactionInputMultisignature::deserialize(in);
    } else {
      throw std::runtime_error("Transaction::deserialize");
    }
  }

  transaction.vout.resize(readVarint<uint16_t>(in));
  for (TransactionOutput& output : transaction.vout) {
    output = TransactionOutput::deserialize(in);
  }

  transaction.extra.resize(readVarint<uint32_t>(in));
  read(in, transaction.extra.data(), transaction.extra.size());
  transaction.signatures.resize(transaction.vin.size());
  for (std::size_t i = 0; i < transaction.vin.size(); ++i) {
    std::size_t signatureCount;
    if (transaction.vin[i].type() == typeid(TransactionInputGenerate)) {
      signatureCount = 0;
    } else if (transaction.vin[i].type() == typeid(TransactionInputToKey)) {
      signatureCount = boost::get<TransactionInputToKey>(transaction.vin[i]).keyOffsets.size();
    } else {
      signatureCount = boost::get<TransactionInputMultisignature>(transaction.vin[i]).signatures;
    }

    transaction.signatures[i].resize(signatureCount);
    for (crypto::signature& signature : transaction.signatures[i]) {
      read(in, &signature, sizeof(signature));
    }
  }

  return transaction;
}

void Block::serialize(IOutputStream& out) const {
  writeVarint(out, majorVersion);
  writeVarint(out, minorVersion);
  if (majorVersion == BLOCK_MAJOR_VERSION_1) {
    writeVarint(out, timestamp);
    write(out, &prevId, sizeof(prevId));
    write(out, nonce);
  } else {
    write(out, &prevId, sizeof(prevId));
    writeVarint(out, parentBlock.majorVersion);
    writeVarint(out, parentBlock.minorVersion);
    writeVarint(out, timestamp);
    write(out, &parentBlock.prevId, sizeof(parentBlock.prevId));
    write(out, nonce);
    writeVarint(out, parentBlock.numberOfTransactions);
    for (const crypto::hash& hash : parentBlock.minerTxBranch) {
      write(out, &hash, sizeof(hash));
    }

    parentBlock.minerTx.serialize(out);
    for (const crypto::hash& hash : parentBlock.blockchainBranch) {
      write(out, &hash, sizeof(hash));
    }
  }

  minerTx.serialize(out);
  writeVarint(out, txHashes.size());
  for (const crypto::hash& hash : txHashes) {
    write(out, &hash, sizeof(hash));
  }
}

Block Block::deserialize(IInputStream& in) {
  Block block;
  readVarint(in, block.majorVersion);
  if (block.majorVersion == BLOCK_MAJOR_VERSION_1) {
    readVarint(in, block.minorVersion);
    if (block.minorVersion != BLOCK_MINOR_VERSION_0 && block.minorVersion != BLOCK_MINOR_VERSION_1) {
      throw std::runtime_error("Invalid block minor version (" + toString(static_cast<uint32_t>(block.minorVersion)) + ") for major version 1");
    }

    readVarint(in, block.timestamp);
    read(in, &block.prevId, sizeof(block.prevId));
    read(in, block.nonce);
  } else if (block.majorVersion == BLOCK_MAJOR_VERSION_2) {
    readVarint(in, block.minorVersion);
    if (block.minorVersion != BLOCK_MINOR_VERSION_0) {
      throw std::runtime_error("Invalid block minor version (" + toString(static_cast<uint32_t>(block.minorVersion)) + ") for major version 2");
    }

    read(in, &block.prevId, sizeof(block.prevId));
    readVarint(in, block.parentBlock.majorVersion);
    if (block.parentBlock.majorVersion != BLOCK_MAJOR_VERSION_1) {
      throw std::runtime_error("Invalid parent block major version (" + toString(static_cast<uint32_t>(block.parentBlock.majorVersion)) + ')');
    }

    readVarint(in, block.parentBlock.minorVersion);
    if (block.parentBlock.minorVersion != BLOCK_MINOR_VERSION_0) {
      throw std::runtime_error("Invalid parent block minor version (" + toString(static_cast<uint32_t>(block.parentBlock.minorVersion)) + ')');
    }


    readVarint(in, block.timestamp);
    read(in, &block.parentBlock.prevId, sizeof(block.parentBlock.prevId));
    read(in, block.nonce);
    readVarint(in, block.parentBlock.numberOfTransactions);

    block.parentBlock.minerTxBranch.resize(crypto::tree_depth(block.parentBlock.numberOfTransactions));
    for (crypto::hash& hash : block.parentBlock.minerTxBranch) {
      read(in, &hash, sizeof(hash));
    }

    block.parentBlock.minerTx = Transaction::deserialize(in);
    tx_extra_merge_mining_tag mergedMiningTag;
    if (!get_mm_tag_from_extra(block.parentBlock.minerTx.extra, mergedMiningTag)) {
      throw std::runtime_error("Cannot get merged mining tag");
    }

    if (mergedMiningTag.depth > 8 * sizeof(crypto::hash)) {
      throw std::runtime_error("Invalid merged mining tag depth (" + toString(mergedMiningTag.depth) + ')');
    }


    block.parentBlock.blockchainBranch.resize(mergedMiningTag.depth);
    for (crypto::hash& hash : block.parentBlock.blockchainBranch) {
      read(in, &hash, sizeof(hash));
    }
  } else {
    throw std::runtime_error("Invalid block major version (" + toString(static_cast<uint32_t>(block.majorVersion)) + ')');
  }

  block.minerTx = Transaction::deserialize(in);
  block.txHashes.resize(readVarint<uint16_t>(in));
  for (crypto::hash& hash : block.txHashes) {
    read(in, &hash, sizeof(hash));
  }

  return block;
}

}
