// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "WalletTransactionSender.h"
#include "WalletUtils.h"

namespace {

uint64_t countNeededMoney(uint64_t fee, const std::vector<CryptoNote::Transfer>& transfers) {
  uint64_t needed_money = fee;
  for (auto& transfer: transfers) {
    CryptoNote::throwIf(transfer.amount == 0, cryptonote::error::ZERO_DESTINATION);
    CryptoNote::throwIf(transfer.amount < 0, cryptonote::error::WRONG_AMOUNT);

    needed_money += transfer.amount;
    CryptoNote::throwIf(static_cast<int64_t>(needed_money) < transfer.amount, cryptonote::error::SUM_OVERFLOW);
  }

  return needed_money;
}

void createChangeDestinations(const cryptonote::account_public_address& address, uint64_t neededMoney, uint64_t foundMoney, cryptonote::tx_destination_entry& changeDts) {
  if (neededMoney < foundMoney) {
    changeDts.addr = address;
    changeDts.amount = foundMoney - neededMoney;
  }
}

void constructTx(const cryptonote::account_keys keys, const std::vector<cryptonote::tx_source_entry>& sources, const std::vector<cryptonote::tx_destination_entry>& splittedDests,
    const std::string& extra, uint64_t unlockTimestamp, uint64_t sizeLimit, cryptonote::transaction& tx) {
  std::vector<uint8_t> extraVec;
  extraVec.reserve(extra.size());
  std::for_each(extra.begin(), extra.end(), [&extraVec] (const char el) { extraVec.push_back(el);});

  bool r = cryptonote::construct_tx(keys, sources, splittedDests, extraVec, tx, unlockTimestamp);
  CryptoNote::throwIf(!r, cryptonote::error::INTERNAL_WALLET_ERROR);
  CryptoNote::throwIf(cryptonote::get_object_blobsize(tx) >= sizeLimit, cryptonote::error::TRANSACTION_SIZE_TOO_BIG);
}

void fillTransactionHash(const cryptonote::transaction& tx, CryptoNote::TransactionHash& hash) {
  crypto::hash h = cryptonote::get_transaction_hash(tx);
  memcpy(hash.data(), reinterpret_cast<const uint8_t *>(&h), hash.size());
}

} //namespace

namespace CryptoNote {

WalletTransactionSender::WalletTransactionSender(WalletUserTransactionsCache& transactionsCache, WalletTxSendingState& sendingTxsStates,
    WalletTransferDetails& transferDetails, WalletUnconfirmedTransactions& unconfirmedTransactions):
      m_transactionsCache(transactionsCache),
      m_sendingTxsStates(sendingTxsStates),
      m_transferDetails(transferDetails),
      m_unconfirmedTransactions(unconfirmedTransactions),
      m_isInitialized(false),
      m_isStoping(false) {
  m_upperTransactionSizeLimit = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE*2 - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE;
}

void WalletTransactionSender::init(cryptonote::account_keys keys) {
  if (m_isInitialized) {
    throw std::system_error(make_error_code(cryptonote::error::NOT_INITIALIZED));
  }

  m_keys = keys;
  m_isInitialized = true;
}

void WalletTransactionSender::stop() {
  m_isStoping = true;
}

std::shared_ptr<WalletRequest> WalletTransactionSender::makeSendRequest(TransactionId& transactionId, std::deque<std::shared_ptr<WalletEvent> >& events,
    const std::vector<Transfer>& transfers, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) {
  if (!m_isInitialized)
    throw std::system_error(make_error_code(cryptonote::error::NOT_INITIALIZED));

  using namespace cryptonote;

  std::shared_ptr<SendTransactionContext> context = std::make_shared<SendTransactionContext>();
  throwIf(transfers.empty(), cryptonote::error::ZERO_DESTINATION);

  TransferId firstTransferId = m_transactionsCache.insertTransfers(transfers);

  uint64_t neededMoney = countNeededMoney(fee, transfers);

  Transaction transaction;
  transaction.firstTransferId = firstTransferId;
  transaction.transferCount = transfers.size();
  transaction.totalAmount = neededMoney;
  transaction.fee = fee;
  transaction.isCoinbase = false;
  transaction.timestamp = 0;
  transaction.extra = extra;
  transaction.blockHeight = UNCONFIRMED_TRANSACTION_HEIGHT;

  TransactionId txId = m_transactionsCache.insertTransaction(std::move(transaction));
  transactionId = txId;
  m_sendingTxsStates.sending(txId);

  context->transactionId = txId;
  context->unlockTimestamp = unlockTimestamp;
  context->mixIn = mixIn;

  context->foundMoney = m_transferDetails.selectTransfersToSend(neededMoney, 0 == mixIn, context->dustPolicy.dustThreshold, context->selectedTransfers);
  throwIf(context->foundMoney < neededMoney, cryptonote::error::WRONG_AMOUNT);

  if(context->mixIn) {
    std::shared_ptr<WalletRequest> request = makeGetRandomOutsRequest(context);
    return request;
  }

  return doSendTransaction(context, events);
}

std::shared_ptr<WalletRequest> WalletTransactionSender::makeGetRandomOutsRequest(std::shared_ptr<SendTransactionContext> context) {
  uint64_t outsCount = context->mixIn + 1;// add one to make possible (if need) to skip real output key
  std::vector<uint64_t> amounts;

  for (auto idx: context->selectedTransfers) {
    const TransferDetails& td = m_transferDetails.getTransferDetails(idx);
    throwIf(td.tx.vout.size() <= td.internalOutputIndex, cryptonote::error::INTERNAL_WALLET_ERROR);
    amounts.push_back(td.amount());
  }

  return std::make_shared<WalletGetRandomOutsByAmountsRequest>(amounts, outsCount, context, std::bind(&WalletTransactionSender::sendTransactionRandomOutsByAmount,
      this, context, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void WalletTransactionSender::sendTransactionRandomOutsByAmount(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletEvent> >& events,
    boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec) {
  if (m_isStoping) {
    events.push_back(std::make_shared<WalletSendTransactionCompletedEvent>(context->transactionId, make_error_code(cryptonote::error::TX_CANCELLED)));
    return;
  }

  if (ec) {
    events.push_back(std::make_shared<WalletSendTransactionCompletedEvent>(context->transactionId, ec));
    return;
  }

  auto scanty_it = std::find_if(context->outs.begin(), context->outs.end(), [&] (cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& out) {return out.outs.size() < context->mixIn;});

  if (scanty_it != context->outs.end()) {
    events.push_back(std::make_shared<WalletSendTransactionCompletedEvent>(context->transactionId, make_error_code(cryptonote::error::MIXIN_COUNT_TOO_BIG)));
    return;
  }

  std::shared_ptr<WalletRequest> req = doSendTransaction(context, events);
  if (req)
    nextRequest = req;
}

std::shared_ptr<WalletRequest> WalletTransactionSender::doSendTransaction(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletEvent> >& events) {
  if (m_isStoping) {
    events.push_back(std::make_shared<WalletSendTransactionCompletedEvent>(context->transactionId, make_error_code(cryptonote::error::TX_CANCELLED)));
    return std::shared_ptr<WalletRequest>();
  }

  try
  {
    Transaction& transaction = m_transactionsCache.getTransaction(context->transactionId);

    std::vector<cryptonote::tx_source_entry> sources;
    prepareInputs(context->selectedTransfers, context->outs, sources, context->mixIn);

    cryptonote::tx_destination_entry changeDts = AUTO_VAL_INIT(changeDts);
    createChangeDestinations(m_keys.m_account_address, transaction.totalAmount, context->foundMoney, changeDts);

    std::vector<cryptonote::tx_destination_entry> splittedDests;
    splitDestinations(transaction.firstTransferId, transaction.transferCount, changeDts, context->dustPolicy, splittedDests);

    cryptonote::transaction tx;
    constructTx(m_keys, sources, splittedDests, transaction.extra, context->unlockTimestamp, m_upperTransactionSizeLimit, tx);

    fillTransactionHash(tx, transaction.hash);

    m_unconfirmedTransactions.add(tx, context->transactionId, changeDts.amount);
    notifyBalanceChanged(events);

    return std::make_shared<WalletRelayTransactionRequest>(tx, std::bind(&WalletTransactionSender::relayTransactionCallback, this, context->transactionId,
          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  }
  catch(std::system_error& ec) {
    events.push_back(std::make_shared<WalletSendTransactionCompletedEvent>(context->transactionId, ec.code()));
  }
  catch(std::exception&) {
    events.push_back(std::make_shared<WalletSendTransactionCompletedEvent>(context->transactionId, make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR)));
  }

  return std::shared_ptr<WalletRequest>();
}

void WalletTransactionSender::relayTransactionCallback(TransactionId txId, std::deque<std::shared_ptr<WalletEvent> >& events,
                                                        boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec) {
  if (m_isStoping) return;

  if (ec) {
    m_sendingTxsStates.error(txId);
  } else {
    m_sendingTxsStates.sent(txId);
  }

  events.push_back(std::make_shared<WalletSendTransactionCompletedEvent>(txId, ec));
}


void WalletTransactionSender::splitDestinations(TransferId firstTransferId, size_t transfersCount, const cryptonote::tx_destination_entry& changeDts,
                                                const TxDustPolicy& dustPolicy, std::vector<cryptonote::tx_destination_entry>& splittedDests) {
  uint64_t dust = 0;

  digitSplitStrategy(firstTransferId, transfersCount, changeDts, dustPolicy.dustThreshold, splittedDests, dust);

  throwIf(dustPolicy.dustThreshold < dust, cryptonote::error::INTERNAL_WALLET_ERROR);
  if (0 != dust && !dustPolicy.addToFee) {
    splittedDests.push_back(cryptonote::tx_destination_entry(dust, dustPolicy.addrForDust));
  }
}


void WalletTransactionSender::digitSplitStrategy(TransferId firstTransferId, size_t transfersCount,
  const cryptonote::tx_destination_entry& change_dst, uint64_t dust_threshold,
  std::vector<cryptonote::tx_destination_entry>& splitted_dsts, uint64_t& dust) {
  splitted_dsts.clear();
  dust = 0;

  for (TransferId idx = firstTransferId; idx < firstTransferId + transfersCount; ++idx) {
    Transfer& de = m_transactionsCache.getTransfer(idx);

    cryptonote::account_public_address addr;
    if (!cryptonote::get_account_address_from_str(addr, de.address)) {
      throw std::system_error(make_error_code(cryptonote::error::BAD_ADDRESS));
    }

    cryptonote::decompose_amount_into_digits(de.amount, dust_threshold,
      [&](uint64_t chunk) { splitted_dsts.push_back(cryptonote::tx_destination_entry(chunk, addr)); },
      [&](uint64_t a_dust) { splitted_dsts.push_back(cryptonote::tx_destination_entry(a_dust, addr)); } );
  }

  cryptonote::decompose_amount_into_digits(change_dst.amount, dust_threshold,
    [&](uint64_t chunk) { splitted_dsts.push_back(cryptonote::tx_destination_entry(chunk, change_dst.addr)); },
    [&](uint64_t a_dust) { dust = a_dust; } );
}


void WalletTransactionSender::prepareInputs(const std::list<size_t>& selectedTransfers, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs,
    std::vector<cryptonote::tx_source_entry>& sources, uint64_t mixIn) {
  size_t i = 0;
  for (size_t idx: selectedTransfers) {
    sources.resize(sources.size()+1);
    cryptonote::tx_source_entry& src = sources.back();
    TransferDetails& td = m_transferDetails.getTransferDetails(idx);
    src.amount = td.amount();

    //paste mixin transaction
    if(outs.size()) {
      outs[i].outs.sort([](const cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& a, const cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& b){return a.global_amount_index < b.global_amount_index;});
      for (auto& daemon_oe: outs[i].outs) {
        if(td.globalOutputIndex == daemon_oe.global_amount_index)
          continue;
        cryptonote::tx_source_entry::output_entry oe;
        oe.first = daemon_oe.global_amount_index;
        oe.second = daemon_oe.out_key;
        src.outputs.push_back(oe);
        if(src.outputs.size() >= mixIn)
          break;
      }
    }

    //paste real transaction to the random index
    auto it_to_insert = std::find_if(src.outputs.begin(), src.outputs.end(), [&](const cryptonote::tx_source_entry::output_entry& a) { return a.first >= td.globalOutputIndex; });

    cryptonote::tx_source_entry::output_entry real_oe;
    real_oe.first = td.globalOutputIndex;
    real_oe.second = boost::get<cryptonote::txout_to_key>(td.tx.vout[td.internalOutputIndex].target).key;

    auto interted_it = src.outputs.insert(it_to_insert, real_oe);

    src.real_out_tx_key = get_tx_pub_key_from_extra(td.tx);
    src.real_output = interted_it - src.outputs.begin();
    src.real_output_in_tx_index = td.internalOutputIndex;
    ++i;
  }
}

void WalletTransactionSender::notifyBalanceChanged(std::deque<std::shared_ptr<WalletEvent> >& events) {
  uint64_t actualBalance = m_transferDetails.countActualBalance();
  uint64_t pendingBalance = m_unconfirmedTransactions.countPendingBalance();
  pendingBalance += m_transferDetails.countPendingBalance();

  events.push_back(std::make_shared<WalletActualBalanceUpdatedEvent>(actualBalance));
  events.push_back(std::make_shared<WalletPendingBalanceUpdatedEvent>(pendingBalance));
}

} /* namespace CryptoNote */
