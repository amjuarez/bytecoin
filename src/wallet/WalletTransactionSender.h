// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_core/account.h"
#include "cryptonote_core/Currency.h"

#include "INode.h"
#include "WalletSendTransactionContext.h"
#include "WalletUserTransactionsCache.h"
#include "WalletUnconfirmedTransactions.h"
#include "WalletRequest.h"

#include "ITransfersContainer.h"

namespace CryptoNote {

class WalletTransactionSender
{
public:
  WalletTransactionSender(const cryptonote::Currency& currency, WalletUserTransactionsCache& transactionsCache, cryptonote::account_keys keys, ITransfersContainer& transfersContainer);

  void init(cryptonote::account_keys keys, ITransfersContainer& transfersContainer);
  void stop();

  std::shared_ptr<WalletRequest> makeSendRequest(TransactionId& transactionId, std::deque<std::shared_ptr<WalletEvent> >& events, const std::vector<Transfer>& transfers,
      uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0);

private:
  std::shared_ptr<WalletRequest> makeGetRandomOutsRequest(std::shared_ptr<SendTransactionContext> context);
  std::shared_ptr<WalletRequest> doSendTransaction(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletEvent> >& events);
  void prepareInputs(const std::list<TransactionOutputInformation>& selectedTransfers, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs,
      std::vector<cryptonote::tx_source_entry>& sources, uint64_t mixIn);
  void splitDestinations(TransferId firstTransferId, size_t transfersCount, const cryptonote::tx_destination_entry& changeDts,
      const TxDustPolicy& dustPolicy, std::vector<cryptonote::tx_destination_entry>& splittedDests);
  void digitSplitStrategy(TransferId firstTransferId, size_t transfersCount, const cryptonote::tx_destination_entry& change_dst, uint64_t dust_threshold,
    std::vector<cryptonote::tx_destination_entry>& splitted_dsts, uint64_t& dust);
  void sendTransactionRandomOutsByAmount(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletEvent> >& events,
      boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec);
  void relayTransactionCallback(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletEvent> >& events,
                                boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec);
  void notifyBalanceChanged(std::deque<std::shared_ptr<WalletEvent> >& events);

  void validateTransfersAddresses(const std::vector<Transfer>& transfers);
  bool validateDestinationAddress(const std::string& address);

  uint64_t selectTransfersToSend(uint64_t neededMoney, bool addDust, uint64_t dust, std::list<TransactionOutputInformation>& selectedTransfers);

  const cryptonote::Currency& m_currency;
  cryptonote::account_keys m_keys;
  WalletUserTransactionsCache& m_transactionsCache;
  uint64_t m_upperTransactionSizeLimit;

  bool m_isStoping;
  ITransfersContainer& m_transferDetails;
};

} /* namespace CryptoNote */
