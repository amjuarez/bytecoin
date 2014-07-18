// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletSynchronizer.h"

#include <algorithm>

#include "WalletErrors.h"
#include "WalletUtils.h"

namespace {

void throwIf(bool expr, cryptonote::error::WalletErrorCodes ec) {
  if (expr)
    throw std::system_error(make_error_code(ec));
}

bool getTxPubKey(const cryptonote::transaction& tx, crypto::public_key& key) {
  std::vector<cryptonote::tx_extra_field> extraFields;
  cryptonote::parse_tx_extra(tx.extra, extraFields);

  cryptonote::tx_extra_pub_key pubKeyField;
  if(!cryptonote::find_tx_extra_field_by_type(extraFields, pubKeyField)) {
    //Public key wasn't found in the transaction extra. Skipping transaction
    return false;
  }

  key = pubKeyField.pub_key;
  return true;
}

void findMyOuts(const cryptonote::account_keys& acc, const cryptonote::transaction& tx, const crypto::public_key& txPubKey, std::vector<size_t>& outs, uint64_t& moneyTransfered) {
  bool r = cryptonote::lookup_acc_outs(acc, tx, txPubKey, outs, moneyTransfered);
  throwIf(!r, cryptonote::error::INTERNAL_WALLET_ERROR);
}

uint64_t countOverallTxOutputs(const cryptonote::transaction& tx) {
  uint64_t amount = 0;
  for (const cryptonote::tx_out& o: tx.vout) {
    amount += o.amount;
  }

  return amount;
}

uint64_t countOverallTxInputs(const cryptonote::transaction& tx) {
  uint64_t amount = 0;
  for (auto& in: tx.vin) {
    if(in.type() != typeid(cryptonote::txin_to_key))
      continue;

    amount += boost::get<cryptonote::txin_to_key>(in).amount;
  }

  return amount;
}

void fillTransactionHash(const cryptonote::transaction& tx, CryptoNote::TransactionHash& hash) {
  crypto::hash h = cryptonote::get_transaction_hash(tx);
  memcpy(hash.data(), reinterpret_cast<const uint8_t *>(&h), hash.size());
}

}

namespace CryptoNote
{

WalletSynchronizer::WalletSynchronizer(const cryptonote::account_base& account, INode& node, std::vector<crypto::hash>& blockchain,
    WalletTransferDetails& transferDetails, WalletUnconfirmedTransactions& unconfirmedTransactions,
    WalletUserTransactionsCache& transactionsCache) :
      m_account(account),
      m_node(node),
      m_blockchain(blockchain),
      m_transferDetails(transferDetails),
      m_unconfirmedTransactions(unconfirmedTransactions),
      m_transactionsCache(transactionsCache),
      m_actualBalance(0),
      m_pendingBalance(0),
      m_isStoping(false) {
}

void WalletSynchronizer::stop() {
  m_isStoping = true;
}

std::shared_ptr<WalletRequest> WalletSynchronizer::makeStartRefreshRequest() {
  std::shared_ptr<SynchronizationContext> context = std::make_shared<SynchronizationContext>();
  std::shared_ptr<WalletRequest> request = makeGetNewBlocksRequest(context);

  return request;
}

void WalletSynchronizer::postGetTransactionOutsGlobalIndicesRequest(ProcessParameters& parameters, const crypto::hash& hash, std::vector<uint64_t>& outsGlobalIndices, uint64_t height) {
  parameters.nextRequest = std::make_shared<WalletGetTransactionOutsGlobalIndicesRequest>(hash, outsGlobalIndices,
                      std::bind(&WalletSynchronizer::handleTransactionOutGlobalIndicesResponse, this, parameters.context, hash, height, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

std::shared_ptr<WalletRequest> WalletSynchronizer::makeGetNewBlocksRequest(std::shared_ptr<SynchronizationContext> context) {
  context->newBlocks.clear();
  context->startHeight = 0;
  context->progress = SynchronizationState();

  std::list<crypto::hash> ids;
  getShortChainHistory(ids);

  std::shared_ptr<WalletRequest>req = std::make_shared<WalletGetNewBlocksRequest>(ids, context, std::bind(&WalletSynchronizer::handleNewBlocksPortion, this,
                                                                                    context, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  return req;
}

void WalletSynchronizer::getShortChainHistory(std::list<crypto::hash>& ids) {
  size_t i = 0;
  size_t current_multiplier = 1;
  size_t sz = m_blockchain.size();

  if(!sz)
    return;

  size_t current_back_offset = 1;
  bool genesis_included = false;

  while(current_back_offset < sz) {
    ids.push_back(m_blockchain[sz-current_back_offset]);
    if(sz-current_back_offset == 0)
      genesis_included = true;
    if(i < 10) {
      ++current_back_offset;
    }else
    {
      current_back_offset += current_multiplier *= 2;
    }
    ++i;
  }

  if(!genesis_included)
    ids.push_back(m_blockchain[0]);
}

void WalletSynchronizer::handleNewBlocksPortion(std::shared_ptr<SynchronizationContext> context, std::deque<std::shared_ptr<WalletEvent> >& events,
    boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec) {
  if (m_isStoping) {
    return;
  }

  if (ec) {
    events.push_back(std::make_shared<WalletSynchronizationProgressUpdatedEvent>(context->startHeight, m_node.getLastLocalBlockHeight(), ec));
    return;
  }

  ProcessParameters parameters;
  parameters.context = context;

  try
  {
    bool fillRequest = processNewBlocks(parameters);

    if (fillRequest) {
      parameters.nextRequest = makeGetNewBlocksRequest(context);
    }
  }
  catch (std::system_error& e) {
    parameters.nextRequest = boost::none;
    parameters.events.push_back(std::make_shared<WalletSynchronizationProgressUpdatedEvent>(context->startHeight, m_node.getLastLocalBlockHeight(), e.code()));
  }
  catch (std::exception&) {
    parameters.nextRequest = boost::none;
    parameters.events.push_back(std::make_shared<WalletSynchronizationProgressUpdatedEvent>(context->startHeight, m_node.getLastLocalBlockHeight(), make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR)));
  }

  refreshBalance(events);

  std::copy(parameters.events.begin(), parameters.events.end(), std::back_inserter(events));
  nextRequest = parameters.nextRequest;
}

//returns true if new request should be performed
bool WalletSynchronizer::processNewBlocks(ProcessParameters& parameters) {
  bool fillRequest = false;
  std::shared_ptr<SynchronizationContext> context = parameters.context;

  size_t currentIndex = context->startHeight + context->progress.blockIdx;

  try
  {
    auto blocksIt = context->newBlocks.begin();
    std::advance(blocksIt, context->progress.blockIdx);

    for (; blocksIt != context->newBlocks.end(); ++blocksIt) {
      if (m_isStoping) return false;

      auto& blockEntry = *blocksIt;

      NextBlockAction action = handleNewBlockchainEntry(parameters, blockEntry, currentIndex);

      if (action == INTERRUPT)
        return false;
      else if (action == CONTINUE)
        fillRequest = true;

      ++context->progress.blockIdx;
      ++currentIndex;
    }
  }
  catch (std::exception& e) {
    context->startHeight = currentIndex;
    throw e;
  }

  return fillRequest;
}

WalletSynchronizer::NextBlockAction WalletSynchronizer::handleNewBlockchainEntry(ProcessParameters& parameters, cryptonote::block_complete_entry& blockEntry, uint64_t height) {
  cryptonote::block b;
  bool r = cryptonote::parse_and_validate_block_from_blob(blockEntry.block, b);
  throwIf(!r, cryptonote::error::INTERNAL_WALLET_ERROR);

  crypto::hash blockId = get_block_hash(b);
  if (height >= m_blockchain.size()) {
    r = processNewBlockchainEntry(parameters, blockEntry, b, blockId, height);

    if (r) {
      parameters.events.push_back(std::make_shared<WalletSynchronizationProgressUpdatedEvent>(height, m_node.getLastLocalBlockHeight(), std::error_code()));
      return CONTINUE;
    }
    return INTERRUPT;
  }

  if(blockId != m_blockchain[height]) {
    //split detected here !!!
    //Wrong daemon response
    throwIf(height == parameters.context->startHeight, cryptonote::error::INTERNAL_WALLET_ERROR);

    detachBlockchain(height);

    r = processNewBlockchainEntry(parameters, blockEntry, b, blockId, height);

    if (r) {
      parameters.events.push_back(std::make_shared<WalletSynchronizationProgressUpdatedEvent>(height, m_node.getLastLocalBlockHeight(), std::error_code()));
      return CONTINUE;
    }
    return INTERRUPT;
  }

  //we already have this block.
  return SKIP;
}

bool WalletSynchronizer::processNewBlockchainEntry(ProcessParameters& parameters, cryptonote::block_complete_entry& blockEntry, const cryptonote::block& b, crypto::hash& blockId, uint64_t height) {
  throwIf(height != m_blockchain.size(), cryptonote::error::INTERNAL_WALLET_ERROR);

  if(b.timestamp + 60*60*24 > m_account.get_createtime()) {
    if (!processMinersTx(parameters, b.miner_tx, height, b.timestamp))
      return false;

    auto txIt = blockEntry.txs.begin();
    std::advance(txIt, parameters.context->progress.transactionIdx);

    for (; txIt != blockEntry.txs.end(); ++txIt) {
      auto& txblob = *txIt;
      cryptonote::transaction tx;

      bool r = parse_and_validate_tx_from_blob(txblob, tx);
      throwIf(!r, cryptonote::error::INTERNAL_WALLET_ERROR);

      r = processNewTransaction(parameters, tx, height, false, b.timestamp);
      parameters.context->progress.transactionIdx++;

      if (!r) return false;
    }
  }

  parameters.context->progress.transactionIdx = 0;

  m_blockchain.push_back(blockId);
  return true;
}

bool WalletSynchronizer::processMinersTx(ProcessParameters& parameters, const cryptonote::transaction& tx, uint64_t height, uint64_t timestamp) {
  bool r = true;

  if (!parameters.context->progress.minersTxProcessed) {
    r = processNewTransaction(parameters, tx, height, true, timestamp);
    parameters.context->progress.minersTxProcessed = true;
  }

  return r;
}

bool WalletSynchronizer::processNewTransaction(ProcessParameters& parameters, const cryptonote::transaction& tx, uint64_t height, bool isCoinbase, uint64_t timestamp) {
  bool res = true;

  processUnconfirmed(parameters, tx, height, timestamp);
  std::vector<size_t> outs;
  uint64_t moneyInMyOuts = 0;

  crypto::public_key publicKey;
  if (!getTxPubKey(tx, publicKey))
    return true; //Public key wasn't found in the transaction extra. Skipping transaction

  findMyOuts(m_account.get_keys(), tx, publicKey, outs, moneyInMyOuts);

  if(!outs.empty() && moneyInMyOuts) {
    fillGetTransactionOutsGlobalIndicesRequest(parameters, tx, outs, publicKey, height);
    res = false;
  }

  uint64_t moneyInMyInputs = processMyInputs(tx);

  if (!moneyInMyOuts && !moneyInMyInputs)
    return res; //There's nothing related to our account, skip it

  updateTransactionsCache(parameters, tx, moneyInMyOuts, moneyInMyInputs, height, isCoinbase, timestamp);

  return res;
}

uint64_t WalletSynchronizer::processMyInputs(const cryptonote::transaction& tx) {
  uint64_t money = 0;
  // check all outputs for spending (compare key images)
  for (auto& in: tx.vin) {
    if(in.type() != typeid(cryptonote::txin_to_key))
      continue;

    size_t idx;
    if (!m_transferDetails.getTransferDetailsIdxByKeyImage(boost::get<cryptonote::txin_to_key>(in).k_image, idx))
      continue;

    money += boost::get<cryptonote::txin_to_key>(in).amount;

    TransferDetails& td = m_transferDetails.getTransferDetails(idx);
    td.spent = true;
  }

  return money;
}

void WalletSynchronizer::fillGetTransactionOutsGlobalIndicesRequest(ProcessParameters& parameters, const cryptonote::transaction& tx,
    const std::vector<size_t>& outs, const crypto::public_key& publicKey, uint64_t height) {
  crypto::hash txid = cryptonote::get_transaction_hash(tx);

  TransactionContextInfo tx_context;
  tx_context.requestedOuts = outs;
  tx_context.transaction = tx;
  tx_context.transactionPubKey = publicKey;

  auto insert_result = parameters.context->transactionContext.emplace(txid, std::move(tx_context));

  postGetTransactionOutsGlobalIndicesRequest(parameters, txid, insert_result.first->second.globalIndices, height);
}

void WalletSynchronizer::updateTransactionsCache(ProcessParameters& parameters, const cryptonote::transaction& tx, uint64_t myOuts, uint64_t myInputs, uint64_t height, bool isCoinbase, uint64_t timestamp) {

  uint64_t allOuts = countOverallTxOutputs(tx);
  uint64_t allInputs = countOverallTxInputs(tx);

  TransactionId foundTx = m_transactionsCache.findTransactionByHash(cryptonote::get_transaction_hash(tx));
  if (foundTx == INVALID_TRANSACTION_ID) {
    Transaction transaction;
    transaction.firstTransferId = INVALID_TRANSFER_ID;
    transaction.transferCount = 0;
    transaction.totalAmount = myOuts - myInputs;
    transaction.fee = isCoinbase ? 0 : allInputs - allOuts;
    fillTransactionHash(tx, transaction.hash);
    transaction.blockHeight = height;
    transaction.isCoinbase = isCoinbase;
    transaction.timestamp = timestamp;

    TransactionId newId = m_transactionsCache.insertTransaction(std::move(transaction));

    parameters.events.push_back(std::make_shared<WalletExternalTransactionCreatedEvent>(newId));
  }
  else
  {
    Transaction& transaction = m_transactionsCache.getTransaction(foundTx);
    transaction.blockHeight = height;
    transaction.timestamp = timestamp;
    transaction.isCoinbase = isCoinbase;

    parameters.events.push_back(std::make_shared<WalletTransactionUpdatedEvent>(foundTx));
  }
}

void WalletSynchronizer::processUnconfirmed(ProcessParameters& parameters, const cryptonote::transaction& tx, uint64_t height, uint64_t timestamp) {
  TransactionId id;
  crypto::hash hash = get_transaction_hash(tx);

  if (!m_unconfirmedTransactions.findTransactionId(hash, id))
    return;

  Transaction& tr = m_transactionsCache.getTransaction(id);
  tr.blockHeight = height;
  tr.timestamp = timestamp;

  m_unconfirmedTransactions.erase(hash);

  parameters.events.push_back(std::make_shared<WalletTransactionUpdatedEvent>(id));
}

void WalletSynchronizer::handleTransactionOutGlobalIndicesResponse(std::shared_ptr<SynchronizationContext> context, crypto::hash txid, uint64_t height,
    std::deque<std::shared_ptr<WalletEvent> >& events, boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec) {
  if (m_isStoping) {
    return;
  }

  if (ec) {
    events.push_back(std::make_shared<WalletSynchronizationProgressUpdatedEvent>(height, m_node.getLastLocalBlockHeight(), ec));
    return;
  }

  try
  {
    auto it = context->transactionContext.find(txid);
    if (it == context->transactionContext.end()) {
      events.push_back(std::make_shared<WalletSynchronizationProgressUpdatedEvent>(height, m_node.getLastLocalBlockHeight(), make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR)));
      return;
    }

    cryptonote::transaction& tx = it->second.transaction;
    std::vector<size_t>& outs = it->second.requestedOuts;
    crypto::public_key& tx_pub_key = it->second.transactionPubKey;
    std::vector<uint64_t>& global_indices = it->second.globalIndices;

    for (size_t o: outs) {
      throwIf(tx.vout.size() <= o, cryptonote::error::INTERNAL_WALLET_ERROR);

      TransferDetails td;
      td.blockHeight = height;
      td.internalOutputIndex = o;
      td.globalOutputIndex = global_indices[o];
      td.tx = tx;
      td.spent = false;
      cryptonote::keypair in_ephemeral;
      cryptonote::generate_key_image_helper(m_account.get_keys(), tx_pub_key, o, in_ephemeral, td.keyImage);
      throwIf(in_ephemeral.pub != boost::get<cryptonote::txout_to_key>(tx.vout[o].target).key, cryptonote::error::INTERNAL_WALLET_ERROR);

      m_transferDetails.addTransferDetails(td);
    }

    context->transactionContext.erase(it);
  }
  catch (std::exception&) {
    events.push_back(std::make_shared<WalletSynchronizationProgressUpdatedEvent>(height, m_node.getLastLocalBlockHeight(), make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR)));
    return;
  }

  handleNewBlocksPortion(context, events, nextRequest, ec);
}

void WalletSynchronizer::detachBlockchain(uint64_t height) {
  m_transferDetails.detachTransferDetails(height);

  m_blockchain.erase(m_blockchain.begin()+height, m_blockchain.end());

  m_transactionsCache.detachTransactions(height);
}

void WalletSynchronizer::refreshBalance(std::deque<std::shared_ptr<WalletEvent> >& events) {
  uint64_t actualBalance = m_transferDetails.countActualBalance();
  uint64_t pendingBalance = m_unconfirmedTransactions.countPendingBalance();
  pendingBalance += m_transferDetails.countPendingBalance();

  if (actualBalance != m_actualBalance) {
    events.push_back(std::make_shared<WalletActualBalanceUpdatedEvent>(actualBalance));
    m_actualBalance = actualBalance;
  }

  if (pendingBalance != m_pendingBalance) {
    events.push_back(std::make_shared<WalletPendingBalanceUpdatedEvent>(pendingBalance));
    m_pendingBalance = pendingBalance;
  }
}

} /* namespace CryptoNote */
