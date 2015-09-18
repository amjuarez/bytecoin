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

#include "IWallet.h"

#include <queue>

#include "IFusionManager.h"
#include "WalletIndices.h"

#include <System/Dispatcher.h>
#include <System/Event.h>
#include "Transfers/TransfersSynchronizer.h"
#include "Transfers/BlockchainSynchronizer.h"

namespace CryptoNote {

class WalletGreen : public IWallet,
                    ITransfersObserver,
                    IBlockchainSynchronizerObserver,
                    IFusionManager {
public:
  WalletGreen(System::Dispatcher& dispatcher, const Currency& currency, INode& node, uint32_t transactionSoftLockTime = 1);
  virtual ~WalletGreen();

  virtual void initialize(const std::string& password) override;
  virtual void initializeWithViewKey(const Crypto::SecretKey& viewSecretKey, const std::string& password) override;
  virtual void load(std::istream& source, const std::string& password) override;
  virtual void shutdown() override;

  virtual void changePassword(const std::string& oldPassword, const std::string& newPassword) override;
  virtual void save(std::ostream& destination, bool saveDetails = true, bool saveCache = true) override;

  virtual size_t getAddressCount() const override;
  virtual std::string getAddress(size_t index) const override;
  virtual KeyPair getAddressSpendKey(size_t index) const override;
  virtual KeyPair getViewKey() const override;
  virtual std::string createAddress() override;
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey) override;
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey) override;
  virtual void deleteAddress(const std::string& address) override;

  virtual uint64_t getActualBalance() const override;
  virtual uint64_t getActualBalance(const std::string& address) const override;
  virtual uint64_t getPendingBalance() const override;
  virtual uint64_t getPendingBalance(const std::string& address) const override;

  virtual size_t getTransactionCount() const override;
  virtual WalletTransaction getTransaction(size_t transactionIndex) const override;
  virtual size_t getTransactionTransferCount(size_t transactionIndex) const override;
  virtual WalletTransfer getTransactionTransfer(size_t transactionIndex, size_t transferIndex) const override;

  virtual size_t transfer(const WalletTransfer& destination, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp  = 0) override;
  virtual size_t transfer(const std::vector<WalletTransfer>& destinations, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0) override;
  virtual size_t transfer(const std::string& sourceAddress, const WalletTransfer& destination, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0) override;
  virtual size_t transfer(const std::string& sourceAddress, const std::vector<WalletTransfer>& destinations, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0) override;

  virtual void start() override;
  virtual void stop() override;
  virtual WalletEvent getEvent() override;

  virtual size_t createFusionTransaction(uint64_t threshold, uint64_t mixin) override;
  virtual bool isFusionTransaction(size_t transactionId) const override;
  virtual IFusionManager::EstimateResult estimate(uint64_t threshold) const override;

protected:
  void throwIfNotInitialized() const;
  void throwIfStopped() const;
  void throwIfTrackingMode() const;
  void doShutdown();
  void clearCaches();
  void initWithKeys(const Crypto::PublicKey& viewPublicKey, const Crypto::SecretKey& viewSecretKey, const std::string& password);
  std::string doCreateAddress(const Crypto::PublicKey& spendPublicKey, const Crypto::SecretKey& spendSecretKey, uint64_t creationTimestamp);

  struct InputInfo {
    TransactionTypes::InputKeyInfo keyInfo;
    WalletRecord* walletRecord = nullptr;
    KeyPair ephKeys;
  };

  struct OutputToTransfer {
    TransactionOutputInformation out;
    WalletRecord* wallet;
  };

  struct ReceiverAmounts {
    CryptoNote::AccountPublicAddress receiver;
    std::vector<uint64_t> amounts;
  };

  struct WalletOuts {
    WalletRecord* wallet;
    std::vector<TransactionOutputInformation> outs;
  };

  virtual void onError(ITransfersSubscription* object, uint32_t height, std::error_code ec) override;
  virtual void onTransactionUpdated(ITransfersSubscription* object, const Crypto::Hash& transactionHash) override;
  void transactionUpdated(ITransfersSubscription* object, const Crypto::Hash& transactionHash);

  virtual void onTransactionDeleted(ITransfersSubscription* object, const Crypto::Hash& transactionHash) override;
  void transactionDeleted(ITransfersSubscription* object, const Crypto::Hash& transactionHash);

  virtual void synchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount) override;
  virtual void synchronizationCompleted(std::error_code result) override;

  void onSynchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount);
  void onSynchronizationCompleted();

  std::vector<WalletOuts> pickWalletsWithMoney() const;
  WalletOuts pickWallet(const std::string& address);

  void updateBalance(CryptoNote::ITransfersContainer* container);
  void unlockBalances(uint32_t height);

  const WalletRecord& getWalletRecord(const Crypto::PublicKey& key) const;
  const WalletRecord& getWalletRecord(const std::string& address) const;
  const WalletRecord& getWalletRecord(CryptoNote::ITransfersContainer* container) const;

  CryptoNote::AccountPublicAddress parseAddress(const std::string& address) const;
  void addWallet(const Crypto::PublicKey& spendPublicKey, const Crypto::SecretKey& spendSecretKey, uint64_t creationTimestamp);
  bool isOutputUsed(const TransactionOutputInformation& out) const;
  void markOutputsSpent(const Crypto::Hash& transactionHash, const std::vector<OutputToTransfer>& selectedTransfers);
  void deleteSpentOutputs(const Crypto::Hash& transactionHash);
  uint64_t countSpentBalance(const WalletRecord* wallet);
  void updateUsedWalletsBalances(const std::vector<OutputToTransfer>& selectedTransfers);
  AccountKeys makeAccountKeys(const WalletRecord& wallet) const;
  size_t getTransactionId(const Crypto::Hash& transactionHash) const;
  void pushEvent(const WalletEvent& event);
  bool isFusionTransaction(const WalletTransaction& walletTx) const;

  size_t doTransfer(std::vector<WalletOuts>&& wallets,
    const std::vector<WalletTransfer>& destinations,
    uint64_t fee,
    uint64_t mixIn,
    const std::string& extra,
    uint64_t unlockTimestamp);

  void requestMixinOuts(const std::vector<OutputToTransfer>& selectedTransfers,
    uint64_t mixIn,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult);

  void prepareInputs(const std::vector<OutputToTransfer>& selectedTransfers,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult,
    uint64_t mixIn,
    std::vector<InputInfo>& keysInfo);

  uint64_t selectTransfers(uint64_t needeMoney,
    bool dust,
    uint64_t dustThreshold,
    std::vector<WalletOuts>&& wallets,
    std::vector<OutputToTransfer>& selectedTransfers);

  void splitDestinations(const std::vector<WalletTransfer>& destinations, const WalletTransfer& changeDestination,
    uint64_t dustThreshold, const Currency& currency, std::vector<ReceiverAmounts>& decomposedOutputs);

  std::unique_ptr<CryptoNote::ITransaction> makeTransaction(const std::vector<ReceiverAmounts>& decomposedOutputs,
    std::vector<InputInfo>& keysInfo, const std::string& extra, uint64_t unlockTimestamp);

  void sendTransaction(ITransaction* tx);

  size_t insertOutgoingTransaction(const Crypto::Hash& transactionHash, int64_t totalAmount, uint64_t fee, const BinaryArray& extra, uint64_t unlockTimestamp);
  bool transactionExists(const Crypto::Hash& hash);
  bool updateWalletTransactionInfo(const Crypto::Hash& hash, const CryptoNote::TransactionInformation& info);
  size_t insertBlockchainTransaction(const TransactionInformation& info, int64_t txBalance);
  void insertIncomingTransfer(size_t txId, const std::string& address, int64_t amount);
  void pushBackOutgoingTransfers(size_t txId, const std::vector<WalletTransfer> &destinations);
  void insertUnlockTransactionJob(const Crypto::Hash& transactionHash, uint32_t blockHeight, CryptoNote::ITransfersContainer* container);
  void deleteUnlockTransactionJob(const Crypto::Hash& transactionHash);

  void unsafeLoad(std::istream& source, const std::string& password);
  void unsafeSave(std::ostream& destination, bool saveDetails, bool saveCache);

  std::vector<OutputToTransfer> pickRandomFusionInputs(uint64_t threshold, size_t minInputCount, size_t maxInputCount);
  ReceiverAmounts decomposeFusionOutputs(uint64_t inputsAmount);
  enum class WalletState {
    INITIALIZED,
    NOT_INITIALIZED
  };

  enum class WalletTrackingMode {
    TRACKING,
    NOT_TRACKING,
    NO_ADDRESSES
  };

  WalletTrackingMode getTrackingMode() const;

  std::pair<WalletTransfers::const_iterator, WalletTransfers::const_iterator> getTransactionTransfers(size_t transactionIndex) const;

  System::Dispatcher& m_dispatcher;
  const Currency& m_currency;
  INode& m_node;
  bool m_stopped = false;

  WalletsContainer m_walletsContainer;
  SpentOutputs m_spentOutputs;
  UnlockTransactionJobs m_unlockTransactionsJob;
  WalletTransactions m_transactions;
  WalletTransfers m_transfers; //sorted
  TransactionChanges m_change;
  mutable std::unordered_map<size_t, bool> m_fusionTxsCache; // txIndex -> isFusion

  BlockchainSynchronizer m_blockchainSynchronizer;
  TransfersSyncronizer m_synchronizer;

  System::Event m_eventOccured;
  std::queue<WalletEvent> m_events;
  mutable System::Event m_readyEvent;

  WalletState m_state = WalletState::NOT_INITIALIZED;

  std::string m_password;

  Crypto::PublicKey m_viewPublicKey;
  Crypto::SecretKey m_viewSecretKey;

  uint64_t m_actualBalance = 0;
  uint64_t m_pendingBalance = 0;

  uint64_t m_upperTransactionSizeLimit;
  uint32_t m_transactionSoftLockTime;
};

} //namespace CryptoNote
