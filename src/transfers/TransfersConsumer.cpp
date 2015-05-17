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

#include "TransfersConsumer.h"
#include "CommonTypes.h"

#include "common/BlockingQueue.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/TransactionApi.h"

#include "IWallet.h"
#include "INode.h"
#include <future>

namespace {

using namespace CryptoNote;

void checkOutputKey(
  const crypto::key_derivation& derivation,
  const PublicKey& key,
  size_t keyIndex,
  size_t outputIndex,
  const std::unordered_set<PublicKey>& spendKeys,
  std::unordered_map<PublicKey, std::vector<uint32_t>>& outputs) {

  PublicKey spendKey;
  crypto::underive_public_key(derivation, keyIndex,
    reinterpret_cast<const crypto::public_key&>(key),
    reinterpret_cast<crypto::public_key&>(spendKey));

  if (spendKeys.find(spendKey) != spendKeys.end()) {
    outputs[spendKey].push_back(static_cast<uint32_t>(outputIndex));
  }

}

void findMyOutputs(
  const ITransactionReader& tx,
  const SecretKey& viewSecretKey,
  const std::unordered_set<PublicKey>& spendKeys,
  std::unordered_map<PublicKey, std::vector<uint32_t>>& outputs) {

  auto txPublicKey = tx.getTransactionPublicKey();
  crypto::key_derivation derivation;

  if (!crypto::generate_key_derivation(
    reinterpret_cast<const crypto::public_key&>(txPublicKey),
    reinterpret_cast<const crypto::secret_key&>(viewSecretKey), 
    derivation)) {
    return;
  }

  size_t keyIndex = 0;
  size_t outputCount = tx.getOutputCount();

  for (size_t idx = 0; idx < outputCount; ++idx) {

    auto outType = tx.getOutputType(size_t(idx));

    if (outType == TransactionTypes::OutputType::Key) {

      TransactionTypes::OutputKey out;
      tx.getOutput(idx, out);
      checkOutputKey(derivation, out.key, keyIndex, idx, spendKeys, outputs);
      ++keyIndex;

    } else if (outType == TransactionTypes::OutputType::Multisignature) {

      TransactionTypes::OutputMultisignature out;
      tx.getOutput(idx, out);
      for (const auto& key : out.keys) {
        checkOutputKey(derivation, key, idx, idx, spendKeys, outputs);
        ++keyIndex;
      }
    }
  }
}

}

namespace CryptoNote {

TransfersConsumer::TransfersConsumer(const cryptonote::Currency& currency, INode& node, const SecretKey& viewSecret) :
  m_node(node), m_viewSecret(viewSecret), m_currency(currency) {
  updateSyncStart();
}

ITransfersSubscription& TransfersConsumer::addSubscription(const AccountSubscription& subscription) {
  if (subscription.keys.viewSecretKey != m_viewSecret) {
    throw std::runtime_error("TransfersConsumer: view secret key mismatch");
  }

  auto& res = m_subscriptions[subscription.keys.address.spendPublicKey];

  if (res.get() == nullptr) {
    res.reset(new TransfersSubscription(m_currency, subscription));
    m_spendKeys.insert(subscription.keys.address.spendPublicKey);
    updateSyncStart();
  }

  return *res;
}

bool TransfersConsumer::removeSubscription(const AccountAddress& address) {
  m_subscriptions.erase(address.spendPublicKey);
  m_spendKeys.erase(address.spendPublicKey);
  updateSyncStart();
  return m_subscriptions.empty();
}

ITransfersSubscription* TransfersConsumer::getSubscription(const AccountAddress& acc) {
  auto it = m_subscriptions.find(acc.spendPublicKey);
  return it == m_subscriptions.end() ? nullptr : it->second.get();
}

void TransfersConsumer::getSubscriptions(std::vector<AccountAddress>& subscriptions) {
  for (const auto& kv : m_subscriptions) {
    subscriptions.push_back(kv.second->getAddress());
  }
}

void TransfersConsumer::updateSyncStart() {
  SynchronizationStart start;

  start.height = std::numeric_limits<uint64_t>::max();
  start.timestamp = std::numeric_limits<uint64_t>::max();

  for (const auto& kv : m_subscriptions) {
    auto subStart = kv.second->getSyncStart();
    start.height = std::min(start.height, subStart.height);
    start.timestamp = std::min(start.timestamp, subStart.timestamp);
  }

  m_syncStart = start;
}

SynchronizationStart TransfersConsumer::getSyncStart() {
  return m_syncStart;
}

void TransfersConsumer::onBlockchainDetach(uint64_t height) {
  for (const auto& kv : m_subscriptions) {
    kv.second->onBlockchainDetach(height);
  }
}

bool TransfersConsumer::onNewBlocks(const CompleteBlock* blocks, uint64_t startHeight, size_t count) {
  assert(blocks);

  struct Tx {
    BlockInfo blockInfo;
    const ITransactionReader* tx;
  };

  struct PreprocessedTx : Tx, PreprocessInfo {};

  std::vector<PreprocessedTx> preprocessedTransactions;
  std::mutex preprocessedTransactionsMutex;

  size_t workers = std::thread::hardware_concurrency();
  if (workers == 0) {
    workers = 2;
  }

  BlockingQueue<Tx> inputQueue(workers * 2);

  std::atomic<bool> stopProcessing(false);

  auto pushingThread = std::async(std::launch::async, [&] {
    for (size_t i = 0; i < count && !stopProcessing; ++i) {
      const auto& block = blocks[i].block;

      if (!block.is_initialized()) {
        continue;
      }

      // filter by syncStartTimestamp
      if (m_syncStart.timestamp && block->timestamp < m_syncStart.timestamp) {
        continue;
      }

      BlockInfo blockInfo;
      blockInfo.height = startHeight + i;
      blockInfo.timestamp = block->timestamp;
      blockInfo.transactionIndex = 0; // position in block

      for (const auto& tx : blocks[i].transactions) {
        auto pubKey = tx->getTransactionPublicKey();
        if (*reinterpret_cast<crypto::public_key*>(&pubKey) == cryptonote::null_pkey) {
          continue;
        }

        Tx item = { blockInfo, tx.get() };
        inputQueue.push(item);
        ++blockInfo.transactionIndex;
      }
    }

    inputQueue.close();
  });

  auto processingFunction = [&] {
    Tx item;
    std::error_code ec;
    while (!stopProcessing && inputQueue.pop(item)) {
      PreprocessedTx output;
      static_cast<Tx&>(output) = item;

      ec = preprocessOutputs(item.blockInfo, *item.tx, output);
      if (ec) {
        stopProcessing = true;
        break;
      }

      std::lock_guard<std::mutex> lk(preprocessedTransactionsMutex);
      preprocessedTransactions.push_back(std::move(output));
    }
    return ec;
  };

  std::vector<std::future<std::error_code>> processingThreads;
  for (size_t i = 0; i < workers; ++i) {
    processingThreads.push_back(std::async(std::launch::async, processingFunction));
  }

  std::error_code processingError;
  for (auto& f : processingThreads) {
    try {
      std::error_code ec = f.get();
      if (!processingError && ec) {
        processingError = ec;
      }
    } catch (const std::system_error& e) {
      processingError = e.code();
    } catch (const std::exception& e) {
      processingError = std::make_error_code(std::errc::operation_canceled);
    }
  }

  if (!processingError) {
    // sort by block height and transaction index in block
    std::sort(preprocessedTransactions.begin(), preprocessedTransactions.end(), [](const PreprocessedTx& a, const PreprocessedTx& b) {
      return std::tie(a.blockInfo.height, a.blockInfo.transactionIndex) < std::tie(b.blockInfo.height, b.blockInfo.transactionIndex);
    });

    for (const auto& tx : preprocessedTransactions) {
      processingError = processTransaction(tx.blockInfo, *tx.tx, tx);
      if (processingError) {
        break;
      }
    }
  } 
  
  if (processingError) {
    forEachSubscription([&](TransfersSubscription& sub) {
      sub.onError(processingError, startHeight);
    });
    return false;
  } 

  auto newHeight = startHeight + count;
  forEachSubscription([newHeight](TransfersSubscription& sub) {
    sub.advanceHeight(newHeight);
  });

  return true;
}

std::error_code TransfersConsumer::onPoolUpdated(const std::vector<cryptonote::Transaction>& addedTransactions, const std::vector<crypto::hash>& deletedTransactions) {
  BlockInfo unconfirmedBlockInfo;
  unconfirmedBlockInfo.timestamp = 0; 
  unconfirmedBlockInfo.height = UNCONFIRMED_TRANSACTION_HEIGHT;
  std::error_code processingError;
  for (auto& cryptonoteTransaction : addedTransactions) {
    auto transaction = CryptoNote::createTransaction(cryptonoteTransaction);
    processingError = processTransaction(unconfirmedBlockInfo, *transaction.get());
    if (processingError) {
      break;
    }
  }

  if (processingError) {
    for (auto& sub : m_subscriptions) {
      sub.second->onError(processingError, UNCONFIRMED_TRANSACTION_HEIGHT);
    }

    return processingError;
  }
  
  for (auto& deletedTxHash : deletedTransactions) {
    for (auto& sub: m_subscriptions) {
      sub.second->deleteUnconfirmedTransaction(*reinterpret_cast<const Hash*>(&deletedTxHash));
    }
  }
  return std::error_code();
}

void TransfersConsumer::getKnownPoolTxIds(std::vector<crypto::hash>& ids) {
  ids.clear();
  std::unordered_set<crypto::hash> knownIds;
  for (auto& sub : m_subscriptions) {
    std::vector<crypto::hash> subscriptionUnconfirmedTxIds;
    sub.second->getContainer().getUnconfirmedTransactions(subscriptionUnconfirmedTxIds);
    knownIds.insert(subscriptionUnconfirmedTxIds.begin(), subscriptionUnconfirmedTxIds.end());
  }

  ids.assign(knownIds.begin(), knownIds.end());
}

std::error_code TransfersConsumer::preprocessOutputs(const BlockInfo& blockInfo, const ITransactionReader& tx, PreprocessInfo& info) {
  findMyOutputs(tx, m_viewSecret, m_spendKeys, info.outputs);

  std::error_code errorCode;
  if (!info.outputs.empty()) {
    auto txHash = tx.getTransactionHash();
    if (blockInfo.height != UNCONFIRMED_TRANSACTION_HEIGHT) {
      errorCode = getGlobalIndices(reinterpret_cast<const crypto::hash&>(txHash), info.globalIdxs);
      if (errorCode) {
        return errorCode;
      }
    }
  }

  return std::error_code();
}

std::error_code TransfersConsumer::processTransaction(const BlockInfo& blockInfo, const ITransactionReader& tx) {
  PreprocessInfo info;
  auto ec = preprocessOutputs(blockInfo, tx, info);
  if (ec) {
    return ec;
  }

  return processTransaction(blockInfo, tx, info);
}


std::error_code TransfersConsumer::processTransaction(const BlockInfo& blockInfo, const ITransactionReader& tx, const PreprocessInfo& info) {
  std::error_code errorCode;
  static std::vector<uint32_t> emptyOutputs;
  for (auto& kv : m_subscriptions) {
    auto it = info.outputs.find(kv.first);
    auto& subscriptionOutputs = (it == info.outputs.end()) ? emptyOutputs : it->second;
    errorCode = processOutputs(blockInfo, *kv.second, tx, subscriptionOutputs, info.globalIdxs);
    if (errorCode) {
      return errorCode;
    }
  }

  return std::error_code();
}

std::error_code TransfersConsumer::processOutputs(const BlockInfo& blockInfo, TransfersSubscription& sub, 
  const ITransactionReader& tx, const std::vector<uint32_t>& outputs, const std::vector<uint64_t>& globalIdxs) {

  if (blockInfo.height != UNCONFIRMED_TRANSACTION_HEIGHT) {
    TransactionInformation subscribtionTxInfo;
    int64_t txBalance;
    if (sub.getContainer().getTransactionInformation(tx.getTransactionHash(), subscribtionTxInfo, txBalance)) {
      if (subscribtionTxInfo.blockHeight == UNCONFIRMED_TRANSACTION_HEIGHT) {
        // pool->blockchain
        sub.markTransactionConfirmed(blockInfo, tx.getTransactionHash(), globalIdxs);
        return std::error_code();
      } else {
        // - Subscription already has this transaction as confirmed, so why are we here?
        // - Because, for instance, some another subscription doesn't have this transactions, so it is given to us again.
        return std::error_code();
      }
    }
  }
  
  std::vector<TransactionOutputInformationIn> transfers;

  auto txPubKey = tx.getTransactionPublicKey();

  for (auto idx : outputs) {

    if (idx >= tx.getOutputCount()) {
      return std::make_error_code(std::errc::argument_out_of_domain);
    }

    auto outType = tx.getOutputType(size_t(idx));

    if (
      outType != TransactionTypes::OutputType::Key &&
      outType != TransactionTypes::OutputType::Multisignature) {
      continue;
    }

    TransactionOutputInformationIn info;

    info.type = outType;
    info.transactionPublicKey = txPubKey;
    info.outputInTransaction = idx;
    info.globalOutputIndex =
      (blockInfo.height == UNCONFIRMED_TRANSACTION_HEIGHT) ?
      UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX :
      globalIdxs[idx];

    if (outType == TransactionTypes::OutputType::Key) {
      TransactionTypes::OutputKey out;
      tx.getOutput(idx, out);

      cryptonote::KeyPair in_ephemeral;
      cryptonote::generate_key_image_helper(
        reinterpret_cast<const cryptonote::account_keys&>(sub.getKeys()),
        reinterpret_cast<const crypto::public_key&>(txPubKey),
        idx,
        in_ephemeral,
        reinterpret_cast<crypto::key_image&>(info.keyImage));

      assert(out.key == reinterpret_cast<const PublicKey&>(in_ephemeral.pub));

      info.amount = out.amount;
      info.outputKey = out.key;

    } else if (outType == TransactionTypes::OutputType::Multisignature) {
      TransactionTypes::OutputMultisignature out;
      tx.getOutput(idx, out);

      info.amount = out.amount;
      info.requiredSignatures = out.requiredSignatures;
    }

    transfers.push_back(info);
  }

  sub.addTransaction(blockInfo, tx, transfers);

  return std::error_code();
}


std::error_code TransfersConsumer::getGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices) {  
  std::promise<std::error_code> prom;
  std::future<std::error_code> f = prom.get_future();

  INode::Callback cb = [&prom](std::error_code ec) { 
    std::promise<std::error_code> p(std::move(prom));
    p.set_value(std::move(ec));
  };

  outsGlobalIndices.clear();
  m_node.getTransactionOutsGlobalIndices(transactionHash, outsGlobalIndices, cb);

  return f.get();
}

}
