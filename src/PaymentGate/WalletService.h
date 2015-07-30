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

#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include "IWallet.h"
#include "INode.h"
#include "CryptoNoteCore/Currency.h"
#include "PaymentServiceJsonRpcMessages.h"
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

struct WalletConfiguration {
  std::string walletFile;
  std::string walletPassword;
};

void generateNewWallet(const CryptoNote::Currency &currency, const WalletConfiguration &conf, Logging::ILogger &logger, System::Dispatcher& dispatcher);

class WalletService {
public:
  typedef std::map<std::string, std::vector<PaymentDetails> > IncomingPayments;

  explicit WalletService(const CryptoNote::Currency& currency, System::Dispatcher& sys, CryptoNote::INode& node, const WalletConfiguration& conf, Logging::ILogger& logger);
  virtual ~WalletService();

  void init();
  void saveWallet();

  std::error_code sendTransaction(const SendTransactionRequest& req, SendTransactionResponse& resp);
  std::error_code getIncomingPayments(const std::vector<std::string>& payments, IncomingPayments& result);
  std::error_code getAddress(size_t index, std::string& address);
  std::error_code getAddressCount(size_t& count);
  std::error_code createAddress(std::string& address);
  std::error_code deleteAddress(const std::string& address);
  std::error_code getActualBalance(const std::string& address, uint64_t& actualBalance);
  std::error_code getPendingBalance(const std::string& address, uint64_t& pendingBalance);
  std::error_code getActualBalance(uint64_t& actualBalance);
  std::error_code getPendingBalance(uint64_t& pendingBalance);
  std::error_code getTransactionsCount(uint64_t& txCount);
  std::error_code getTransfersCount(uint64_t& trCount);
  std::error_code getTransactionByTransferId(size_t transfer, size_t& transaction);
  std::error_code getTransaction(size_t txId, bool& found, TransactionRpcInfo& rpcInfo);
  std::error_code listTransactions(size_t startingTxId, uint32_t maxTxCount, std::vector<TransactionRpcInfo>& txsRpcInfo);
  std::error_code getTransfer(size_t txId, bool& found, TransferRpcInfo& rpcInfo);

private:
  void refresh();

  void loadWallet();
  void loadPaymentsCacheAndTransferIndices();
  void insertTransaction(size_t id, const Crypto::Hash& paymentIdBin, bool confirmed);

  void fillTransactionRpcInfo(size_t txId, const CryptoNote::WalletTransaction& tx, TransactionRpcInfo& rpcInfo);
  void makeTransfers(const std::vector<TransferDestination>& destinations, std::vector<CryptoNote::WalletTransfer>& transfers);

  struct PaymentItem {
    std::string paymentId;
    size_t transactionId;
    bool confirmed;
  };

  typedef boost::multi_index::hashed_unique<BOOST_MULTI_INDEX_MEMBER(PaymentItem, size_t, transactionId)> TxIdIndex;
  typedef boost::multi_index::hashed_non_unique<BOOST_MULTI_INDEX_MEMBER(PaymentItem, std::string, paymentId)> PaymentIndex;
  typedef boost::multi_index::multi_index_container<
    PaymentItem,
    boost::multi_index::indexed_by<
      TxIdIndex,
      PaymentIndex
    >
  > PaymentsContainer;

  std::unique_ptr<CryptoNote::IWallet > wallet;
  CryptoNote::INode* node;
  const WalletConfiguration& config;
  bool inited;
  Logging::LoggerRef logger;
  std::vector<size_t> transfersIndices;
  System::Dispatcher& dispatcher;
  System::ContextGroup refreshContext;

  PaymentsContainer paymentsCache;
  PaymentsContainer::nth_index<0>::type& txIdIndex;
  PaymentsContainer::nth_index<1>::type& paymentIdIndex;
};

} //namespace PaymentService
