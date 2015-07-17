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

#pragma once

#include <System/Dispatcher.h>
#include "PaymentServiceConfiguration.h"
#include "IWallet.h"
#include "INode.h"
#include "WalletObservers.h"
#include "cryptonote_core/Currency.h"
#include "JsonRpcMessages.h"
#undef ERROR //TODO: workaround for windows build. fix it
#include "Logging/LoggerRef.h"

#include <fstream>
#include <memory>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace PaymentService {

struct SendTransactionRequest;
struct SendTransactionResponse;
struct TransferDestination;
struct TransactionRpcInfo;
struct TransferRpcInfo;

void importLegacyKeys(const Configuration& conf);
void generateNewWallet (CryptoNote::Currency &currency, const Configuration &conf, Logging::ILogger &logger);

class WalletService : public CryptoNote::IWalletObserver {
public:
  typedef std::map<std::string, std::vector<PaymentDetails> > IncomingPayments;

  explicit WalletService(const CryptoNote::Currency& currency, System::Dispatcher& sys, CryptoNote::INode& node, const Configuration& conf, Logging::ILogger& logger);
  virtual ~WalletService();

  void init();
  void saveWallet();

  std::error_code sendTransaction(const SendTransactionRequest& req, SendTransactionResponse& resp);
  std::error_code getIncomingPayments(const std::vector<std::string>& payments, IncomingPayments& result);
  std::error_code getAddress(std::string& address);
  std::error_code getActualBalance(uint64_t& actualBalance);
  std::error_code getPendingBalance(uint64_t& pendingBalance);
  std::error_code getTransactionsCount(uint64_t& txCount);
  std::error_code getTransfersCount(uint64_t& trCount);
  std::error_code getTransactionByTransferId(CryptoNote::TransferId transfer, CryptoNote::TransactionId& transaction);
  std::error_code getTransaction(CryptoNote::TransactionId txId, bool& found, TransactionRpcInfo& rpcInfo);
  std::error_code listTransactions(CryptoNote::TransactionId startingTxId, uint32_t maxTxCount, std::vector<TransactionRpcInfo>& txsRpcInfo);
  std::error_code getTransfer(CryptoNote::TransferId txId, bool& found, TransferRpcInfo& rpcInfo);

private:
  void loadWallet();
  void loadPaymentsCache();
  void insertTransaction(CryptoNote::TransactionId id, const crypto::hash& paymentIdBin);

  void makeTransfers(const std::vector<TransferDestination>& destinations, std::vector<CryptoNote::Transfer>& transfers);
  void fillTransactionRpcInfo(const CryptoNote::TransactionInfo& txInfo, TransactionRpcInfo& rpcInfo);
  void fillTransferRpcInfo(const CryptoNote::Transfer& transfer, TransferRpcInfo& rpcInfo);

  virtual void externalTransactionCreated(CryptoNote::TransactionId transactionId);
  virtual void transactionUpdated(CryptoNote::TransactionId transactionId);

  struct PaymentItem {
    std::string paymentId;
    CryptoNote::TransactionId transactionId;
  };

  typedef boost::multi_index::hashed_unique<BOOST_MULTI_INDEX_MEMBER(PaymentItem, CryptoNote::TransactionId, transactionId)> TxIdIndex;
  typedef boost::multi_index::hashed_non_unique<BOOST_MULTI_INDEX_MEMBER(PaymentItem, std::string, paymentId)> PaymentIndex;
  typedef boost::multi_index::multi_index_container<
    PaymentItem,
    boost::multi_index::indexed_by<
      TxIdIndex,
      PaymentIndex
    >
  > PaymentsContainer;

  std::unique_ptr<CryptoNote::IWallet> wallet;
  CryptoNote::INode* node;
  const Configuration& config;
  bool inited;
  WalletTransactionSendObserver sendObserver;
  Logging::LoggerRef logger;

  PaymentsContainer paymentsCache;
  PaymentsContainer::nth_index<0>::type& txIdIndex;
  PaymentsContainer::nth_index<1>::type& paymentIdIndex;
};

} //namespace PaymentService
