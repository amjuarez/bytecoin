// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <list>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "IWallet.h"
#include "INode.h"
#include "WalletErrors.h"
#include "WalletAsyncContextCounter.h"
#include "common/ObserverManager.h"
#include "cryptonote_core/tx_extra.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/Currency.h"
#include "WalletUserTransactionsCache.h"
#include "WalletUnconfirmedTransactions.h"

#include "WalletTransactionSender.h"
#include "WalletRequest.h"

#include "transfers/BlockchainSynchronizer.h"
#include "transfers/TransfersSynchronizer.h"

namespace CryptoNote {

class SyncStarter;

class Wallet : 
  public IWallet, 
  IBlockchainSynchronizerObserver,  
  ITransfersObserver {

public:
  Wallet(const cryptonote::Currency& currency, INode& node);
  virtual ~Wallet();

  virtual void addObserver(IWalletObserver* observer);
  virtual void removeObserver(IWalletObserver* observer);

  virtual void initAndGenerate(const std::string& password);
  virtual void initAndLoad(std::istream& source, const std::string& password);
  virtual void initWithKeys(const WalletAccountKeys& accountKeys, const std::string& password);
  virtual void shutdown();
  virtual void reset();

  virtual void save(std::ostream& destination, bool saveDetailed = true, bool saveCache = true);

  virtual std::error_code changePassword(const std::string& oldPassword, const std::string& newPassword);

  virtual std::string getAddress();

  virtual uint64_t actualBalance();
  virtual uint64_t pendingBalance();

  virtual size_t getTransactionCount();
  virtual size_t getTransferCount();

  virtual TransactionId findTransactionByTransferId(TransferId transferId);

  virtual bool getTransaction(TransactionId transactionId, TransactionInfo& transaction);
  virtual bool getTransfer(TransferId transferId, Transfer& transfer);

  virtual TransactionId sendTransaction(const Transfer& transfer, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0);
  virtual TransactionId sendTransaction(const std::vector<Transfer>& transfers, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0);
  virtual std::error_code cancelTransaction(size_t transactionId);

  virtual void getAccountKeys(WalletAccountKeys& keys);

private:

  // IBlockchainSynchronizerObserver
  virtual void synchronizationProgressUpdated(uint64_t current, uint64_t total) override;
  virtual void synchronizationCompleted(std::error_code result) override;

  // ITransfersObserver
  virtual void onTransactionUpdated(ITransfersSubscription* object, const Hash& transactionHash) override;
  virtual void onTransactionDeleted(ITransfersSubscription* object, const Hash& transactionHash) override;

  void initSync();
  void throwIfNotInitialised();

  void doSave(std::ostream& destination, bool saveDetailed, bool saveCache);
  void doLoad(std::istream& source);

  crypto::chacha8_iv encrypt(const std::string& plain, std::string& cipher);
  void decrypt(const std::string& cipher, std::string& plain, crypto::chacha8_iv iv, const std::string& password);

  void synchronizationCallback(WalletRequest::Callback callback, std::error_code ec);
  void sendTransactionCallback(WalletRequest::Callback callback, std::error_code ec);
  void notifyClients(std::deque<std::shared_ptr<WalletEvent> >& events);
  void notifyIfBalanceChanged();

  enum WalletState
  {
    NOT_INITIALIZED = 0,
    INITIALIZED,
    LOADING,
    SAVING
  };

  WalletState m_state;
  std::mutex m_cacheMutex;
  cryptonote::account_base m_account;
  std::string m_password;
  const cryptonote::Currency& m_currency;
  INode& m_node;
  bool m_isStopping;

  std::atomic<uint64_t> m_lastNotifiedActualBalance;
  std::atomic<uint64_t> m_lastNotifiedPendingBalance;

  BlockchainSynchronizer m_blockchainSync;
  TransfersSyncronizer m_transfersSync;
  ITransfersContainer* m_transferDetails;

  WalletUserTransactionsCache m_transactionsCache;
  std::unique_ptr<WalletTransactionSender> m_sender;

  WalletAsyncContextCounter m_asyncContextCounter;
  tools::ObserverManager<CryptoNote::IWalletObserver> m_observerManager;

  std::unique_ptr<SyncStarter> m_onInitSyncStarter;
};

} //namespace CryptoNote
