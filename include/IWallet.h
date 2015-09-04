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

#include <limits>
#include <string>
#include <vector>
#include "CryptoNote.h"

namespace CryptoNote {

const size_t WALLET_INVALID_TRANSACTION_ID = std::numeric_limits<size_t>::max();
const size_t WALLET_INVALID_TRANSFER_ID = std::numeric_limits<size_t>::max();
const uint32_t WALLET_UNCONFIRMED_TRANSACTION_HEIGHT = std::numeric_limits<uint32_t>::max();

enum class WalletTransactionState : uint8_t {
  SUCCEEDED = 0,
  FAILED,
  CANCELLED
};

enum WalletEventType {
  TRANSACTION_CREATED,
  TRANSACTION_UPDATED,
  BALANCE_UNLOCKED,
  SYNC_PROGRESS_UPDATED,
  SYNC_COMPLETED
};

struct WalletTransactionCreatedData {
  size_t transactionIndex;
};

struct WalletTransactionUpdatedData {
  size_t transactionIndex;
};

struct WalletSynchronizationProgressUpdated {
  uint32_t processedBlockCount;
  uint32_t totalBlockCount;
};

struct WalletEvent {
  WalletEventType type;
  union {
    WalletTransactionCreatedData transactionCreated;
    WalletTransactionUpdatedData transactionUpdated;
    WalletSynchronizationProgressUpdated synchronizationProgressUpdated;
  };
};

struct WalletTransaction {
  WalletTransactionState state;
  uint64_t timestamp;
  uint32_t blockHeight;
  Crypto::Hash hash;
  int64_t totalAmount;
  uint64_t fee;
  uint64_t creationTime;
  uint64_t unlockTime;
  std::string extra;
  bool isBase;
};

struct WalletTransfer {
  std::string address;
  int64_t amount;
};

class IWallet {
public:
  virtual ~IWallet() {}

  virtual void initialize(const std::string& password) = 0;
  virtual void initializeWithViewKey(const Crypto::SecretKey& viewSecretKey, const std::string& password) = 0;
  virtual void load(std::istream& source, const std::string& password) = 0;
  virtual void shutdown() = 0;

  virtual void changePassword(const std::string& oldPassword, const std::string& newPassword) = 0;
  virtual void save(std::ostream& destination, bool saveDetails = true, bool saveCache = true) = 0;

  virtual size_t getAddressCount() const = 0;
  virtual std::string getAddress(size_t index) const = 0;
  virtual KeyPair getAddressSpendKey(size_t index) const = 0;
  virtual KeyPair getViewKey() const = 0;
  virtual std::string createAddress() = 0;
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey) = 0;
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey) = 0;
  virtual void deleteAddress(const std::string& address) = 0;

  virtual uint64_t getActualBalance() const = 0;
  virtual uint64_t getActualBalance(const std::string& address) const = 0;
  virtual uint64_t getPendingBalance() const = 0;
  virtual uint64_t getPendingBalance(const std::string& address) const = 0;

  virtual size_t getTransactionCount() const = 0;
  virtual WalletTransaction getTransaction(size_t transactionIndex) const = 0;
  virtual size_t getTransactionTransferCount(size_t transactionIndex) const = 0;
  virtual WalletTransfer getTransactionTransfer(size_t transactionIndex, size_t transferIndex) const = 0;

  virtual size_t transfer(const WalletTransfer& destination, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0) = 0;
  virtual size_t transfer(const std::vector<WalletTransfer>& destinations, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0) = 0;
  virtual size_t transfer(const std::string& sourceAddress, const WalletTransfer& destination, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0) = 0;
  virtual size_t transfer(const std::string& sourceAddress, const std::vector<WalletTransfer>& destinations, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0) = 0;

  virtual void start() = 0;
  virtual void stop() = 0;

  //blocks until an event occurred
  virtual WalletEvent getEvent() = 0;
};

}
