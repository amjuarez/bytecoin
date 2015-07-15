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

#include "BlockchainExplorerDataBuilder.h"

#include <boost/utility/value_init.hpp>
#include <boost/range/combine.hpp>

#include "Common/StringTools.h"
#include "cryptonote_core/cryptonote_format_utils.h"

namespace CryptoNote {

BlockchainExplorerDataBuilder::BlockchainExplorerDataBuilder(CryptoNote::ICore& core, CryptoNote::ICryptonoteProtocolQuery& protocol) :
  core(core),
  protocol(protocol)
{
}

bool BlockchainExplorerDataBuilder::getMixin(const Transaction& transaction, uint64_t& mixin) {
  mixin = 0;
  for (const TransactionInput& txin : transaction.vin) {
    if (txin.type() != typeid(TransactionInputToKey)) {
      continue;
    }
    uint64_t currentMixin = boost::get<TransactionInputToKey>(txin).keyOffsets.size();
    if (currentMixin > mixin) {
      mixin = currentMixin;
    }
  }
  return true;
}

bool BlockchainExplorerDataBuilder::getPaymentId(const Transaction& transaction, crypto::hash& paymentId) {
  std::vector<tx_extra_field> txExtraFields;
  parse_tx_extra(transaction.extra, txExtraFields);
  tx_extra_nonce extraNonce;
  if (!find_tx_extra_field_by_type(txExtraFields, extraNonce)) {
    return false;
  }
  return get_payment_id_from_tx_extra_nonce(extraNonce.nonce, paymentId);
}

bool BlockchainExplorerDataBuilder::fillTxExtra(const std::vector<uint8_t>& rawExtra, TransactionExtraDetails& extraDetails) {
  extraDetails.raw = rawExtra;
  std::vector<tx_extra_field> txExtraFields;
  parse_tx_extra(rawExtra, txExtraFields);
  for (const tx_extra_field& field : txExtraFields) {
    if (typeid(tx_extra_padding) == field.type()) {
      extraDetails.padding.push_back(std::move(boost::get<tx_extra_padding>(field).size));
    }
    else if (typeid(tx_extra_pub_key) == field.type()) {
      extraDetails.publicKey.push_back(std::move(reinterpret_cast<const std::array<uint8_t, 16>&>(boost::get<tx_extra_pub_key>(field).pub_key)));
    }
    else if (typeid(tx_extra_nonce) == field.type()) {
      extraDetails.nonce.push_back(std::move(Common::toHex(boost::get<tx_extra_nonce>(field).nonce.data(), boost::get<tx_extra_nonce>(field).nonce.size())));
    }
  }
  return true;
}

size_t BlockchainExplorerDataBuilder::median(std::vector<size_t>& v) {
  if(v.empty())
    return boost::value_initialized<size_t>();
  if(v.size() == 1)
    return v[0];

  size_t n = (v.size()) / 2;
  std::sort(v.begin(), v.end());
  //nth_element(v.begin(), v.begin()+n-1, v.end());
  if(v.size()%2)
  {//1, 3, 5...
    return v[n];
  }else 
  {//2, 4, 6...
    return (v[n-1] + v[n])/2;
  }

}

bool BlockchainExplorerDataBuilder::fillBlockDetails(const Block &block, BlockDetails& blockDetails) {
  crypto::hash hash = get_block_hash(block);
  
  blockDetails.majorVersion = block.majorVersion;
  blockDetails.minorVersion = block.minorVersion;
  blockDetails.timestamp = block.timestamp;
  blockDetails.prevBlockHash = reinterpret_cast<const std::array<uint8_t, 32>&>(block.prevId);
  blockDetails.nonce = block.nonce;
  blockDetails.hash = reinterpret_cast<const std::array<uint8_t, 32>&>(hash);

  blockDetails.reward = 0;
  for (const TransactionOutput& out : block.minerTx.vout) {
    blockDetails.reward += out.amount;
  }

  if (block.minerTx.vin.front().type() != typeid(TransactionInputGenerate))
    return false;
  blockDetails.height = boost::get<TransactionInputGenerate>(block.minerTx.vin.front()).height;
  
  crypto::hash tmpHash = core.getBlockIdByHeight(blockDetails.height);
  blockDetails.isOrphaned = hash != tmpHash;
  
  if (!core.getBlockDifficulty(blockDetails.height, blockDetails.difficulty)) {
    return false;
  }

  std::vector<size_t> blocksSizes;
  if (!core.getBackwardBlocksSizes(blockDetails.height, blocksSizes, parameters::CRYPTONOTE_REWARD_BLOCKS_WINDOW)) {
    return false;
  }
  blockDetails.sizeMedian = median(blocksSizes);

  size_t blockSize = 0;
  if (!core.getBlockSize(hash, blockSize)) {
    return false;
  }
  blockDetails.transactionsCumulativeSize = blockSize;

  size_t blokBlobSize = get_object_blobsize(block);
  size_t minerTxBlobSize = get_object_blobsize(block.minerTx);
  blockDetails.blockSize = blokBlobSize + blockDetails.transactionsCumulativeSize - minerTxBlobSize;
  
  if (!core.getAlreadyGeneratedCoins(hash, blockDetails.alreadyGeneratedCoins)) {
    return false;
  }
  
  blockDetails.alreadyGeneratedTransactions = 0; //TODO

  uint64_t prevBlockGeneratedCoins = 0;
  if (blockDetails.height > 0)
  {
    if (!core.getAlreadyGeneratedCoins(block.prevId, prevBlockGeneratedCoins)) {
      return false;
    }
  }
  uint64_t maxReward = 0;
  uint64_t currentReward = 0;
  int64_t emissionChange = 0;
  bool penalizeFee = block.majorVersion >= 2;
  if(!core.getBlockReward(blockDetails.sizeMedian, 0, prevBlockGeneratedCoins, 0, penalizeFee, maxReward, emissionChange))
  {
    return false;
  }
  if(!core.getBlockReward(blockDetails.sizeMedian, blockDetails.transactionsCumulativeSize, prevBlockGeneratedCoins, 0, penalizeFee, currentReward, emissionChange))
  {
    return false;
  }

  blockDetails.baseReward = maxReward;
  if (maxReward == 0 && currentReward == 0)
  {
    blockDetails.penalty = static_cast<double>(0);
  }
  else
  {
    if (maxReward < currentReward) {
      return false;
    }
    blockDetails.penalty = static_cast<double>(maxReward - currentReward) / static_cast<double>(maxReward);
  }

  
  blockDetails.transactions.reserve(block.txHashes.size() + 1);
  TransactionDetails transactionDetails;
  if (!fillTransactionDetails(block.minerTx, transactionDetails, block.timestamp)) {
    return false;
  }
  blockDetails.transactions.push_back(std::move(transactionDetails));
  
  std::list<Transaction> found;
  std::list<crypto::hash> missed;
  core.getTransactions(block.txHashes, found, missed);
  if (found.size() != block.txHashes.size()) {
    return false;
  }
  
  blockDetails.totalFeeAmount = 0;
    
  for (const Transaction& tx : found) {
    TransactionDetails transactionDetails;
    if (!fillTransactionDetails(tx, transactionDetails, block.timestamp)) {
      return false;
    }
    blockDetails.transactions.push_back(std::move(transactionDetails));
    blockDetails.totalFeeAmount += transactionDetails.fee;
  }
  return true;
}

bool BlockchainExplorerDataBuilder::fillTransactionDetails(const Transaction& transaction, TransactionDetails& transactionDetails, uint64_t timestamp) {
  crypto::hash hash = get_transaction_hash(transaction);
  transactionDetails.hash = reinterpret_cast<const std::array<uint8_t, 32>&>(hash);
  
  transactionDetails.timestamp = timestamp;
  
  crypto::hash blockHash;
  uint64_t blockHeight;
  if (!core.getBlockContainingTx(hash, blockHash, blockHeight)) {
    transactionDetails.inBlockchain = false;
    transactionDetails.blockHeight = boost::value_initialized<uint64_t>();
    transactionDetails.blockHash = boost::value_initialized<std::array<uint8_t, 32>>();
  } else {
    transactionDetails.inBlockchain = true;
    transactionDetails.blockHeight = blockHeight;
    transactionDetails.blockHash = reinterpret_cast<const std::array<uint8_t, 32>&>(blockHash);
    if (timestamp == 0) {
      Block block;
      if (!core.getBlockByHash(blockHash, block)) {
        return false;
      }
      transactionDetails.timestamp = block.timestamp;
    }
  }
  
  transactionDetails.size = get_object_blobsize(transaction);
  transactionDetails.unlockTime = transaction.unlockTime;
  transactionDetails.totalOutputsAmount = get_outs_money_amount(transaction);

  uint64_t inputsAmount;
  if (!get_inputs_money_amount(transaction, inputsAmount)) {
    return false;
  }
  transactionDetails.totalInputsAmount = inputsAmount;

  if (transaction.vin.size() > 0 && transaction.vin.front().type() == typeid(TransactionInputGenerate)) {
    //It's gen transaction
    transactionDetails.fee = 0;
    transactionDetails.mixin = 0;
  } else {
    uint64_t fee;
    if (!get_tx_fee(transaction, fee)) {
      return false;
    }
    transactionDetails.fee = fee;
    uint64_t mixin;
    if (!getMixin(transaction, mixin)) {
      return false;
    }
    transactionDetails.mixin = mixin;
  }
  
  crypto::hash paymentId;
  if (getPaymentId(transaction, paymentId)) {
    transactionDetails.paymentId = reinterpret_cast<const std::array<uint8_t, 32>&>(paymentId);
  }
  else {
    transactionDetails.paymentId = boost::value_initialized<std::array<uint8_t, 32>>();
  }
  
  fillTxExtra(transaction.extra, transactionDetails.extra);
  
  transactionDetails.signatures.reserve(transaction.signatures.size());
  for (const std::vector<crypto::signature>& signatures : transaction.signatures) {
    std::vector<std::array<uint8_t, 32>> signaturesDetails;
    signaturesDetails.reserve(signatures.size());
    for (const crypto::signature& signature : signatures) {
      signaturesDetails.push_back(std::move(reinterpret_cast<const std::array<uint8_t, 32>&>(signature)));
    }
    transactionDetails.signatures.push_back(std::move(signaturesDetails));
  }
  
  transactionDetails.inputs.reserve(transaction.vin.size());
  for (const TransactionInput& txIn : transaction.vin) {
    TransactionInputDetails txInDetails;

    if (txIn.type() == typeid(TransactionInputGenerate)) {
      TransactionInputGenerateDetails txInGenDetails;
      txInGenDetails.height = boost::get<TransactionInputGenerate>(txIn).height;
      txInDetails.amount = 0;
      for (const TransactionOutput& out : transaction.vout) {
        txInDetails.amount += out.amount;
      }
      txInDetails.input = txInGenDetails;
    } else if (txIn.type() == typeid(TransactionInputToKey)) {
      TransactionInputToKeyDetails txInToKeyDetails;
      const TransactionInputToKey& txInToKey = boost::get<TransactionInputToKey>(txIn);
      std::list<std::pair<crypto::hash, size_t>> outputReferences;
      if (!core.scanOutputkeysForIndices(txInToKey, outputReferences)) {
        return false;
      }
      txInDetails.amount = txInToKey.amount;
      txInToKeyDetails.keyOffsets = txInToKey.keyOffsets;
      txInToKeyDetails.keyImage = reinterpret_cast<const std::array<uint8_t, 16>&>(txInToKey.keyImage);
      txInToKeyDetails.mixin = txInToKey.keyOffsets.size();
      txInToKeyDetails.output.number = outputReferences.back().second;
      txInToKeyDetails.output.transactionHash = reinterpret_cast<const std::array<uint8_t, 32>&>(outputReferences.back().first);
      txInDetails.input = txInToKeyDetails;
    } else if (txIn.type() == typeid(TransactionInputMultisignature)) {
      TransactionInputMultisignatureDetails txInMultisigDetails;
      const TransactionInputMultisignature& txInMultisig = boost::get<TransactionInputMultisignature>(txIn);
      txInDetails.amount = txInMultisig.amount;
      txInMultisigDetails.signatures = txInMultisig.signatures;
      std::pair<crypto::hash, size_t> outputReference;
      if (!core.getMultisigOutputReference(txInMultisig, outputReference)) {
        return false;
      }
      txInMultisigDetails.output.number = outputReference.second;
      txInMultisigDetails.output.transactionHash = reinterpret_cast<const std::array<uint8_t, 32>&>(outputReference.first);
      txInDetails.input = txInMultisigDetails;
    } else {
      return false;
    }
    transactionDetails.inputs.push_back(std::move(txInDetails));
  }
  
  transactionDetails.outputs.reserve(transaction.vout.size());
  std::vector<uint64_t> globalIndices;
  globalIndices.reserve(transaction.vout.size());
  if (!core.get_tx_outputs_gindexs(hash, globalIndices)) {
    for (size_t i = 0; i < transaction.vout.size(); ++i) {
      globalIndices.push_back(0);
    }
  }

  typedef boost::tuple<TransactionOutput, uint64_t> outputWithIndex;
  auto range = boost::combine(transaction.vout, globalIndices);
  for (const outputWithIndex& txOutput : range) {
    TransactionOutputDetails txOutDetails;
    txOutDetails.amount = txOutput.get<0>().amount;
    txOutDetails.globalIndex = txOutput.get<1>();

    if (txOutput.get<0>().target.type() == typeid(TransactionOutputToKey)) {
      TransactionOutputToKeyDetails txOutToKeyDetails;
      txOutToKeyDetails.txOutKey = reinterpret_cast<const std::array<uint8_t, 16>&>(boost::get<TransactionOutputToKey>(txOutput.get<0>().target).key);
      txOutDetails.output = txOutToKeyDetails;
    } else if (txOutput.get<0>().target.type() == typeid(TransactionOutputMultisignature)) {
      TransactionOutputMultisignatureDetails txOutMultisigDetails;
      TransactionOutputMultisignature txOutMultisig = boost::get<TransactionOutputMultisignature>(txOutput.get<0>().target);
      txOutMultisigDetails.keys.reserve(txOutMultisig.keys.size());
      for (const crypto::public_key& key : txOutMultisig.keys) {
        txOutMultisigDetails.keys.push_back(std::move(reinterpret_cast<const std::array<uint8_t, 16>&>(key)));
      }
      txOutMultisigDetails.requiredSignatures = txOutMultisig.requiredSignatures;
      txOutDetails.output = txOutMultisigDetails;
    } else {
      return false;
    }
    transactionDetails.outputs.push_back(std::move(txOutDetails));
  }
  
  return true;
}

}
