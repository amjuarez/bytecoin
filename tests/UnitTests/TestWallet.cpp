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

#include "gtest/gtest.h"

#include <system_error>
#include <chrono>
#include <numeric>

#include "Common/StringTools.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "CryptoNoteCore/TransactionApiExtra.h"
#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"
#include "TransactionApiHelpers.h"
#include <Logging/ConsoleLogger.h>
#include "Wallet/WalletGreen.h"
#include "WalletLegacy/WalletUserTransactionsCache.h"
#include "WalletLegacy/WalletLegacySerializer.h"
#include <System/Dispatcher.h>
#include <System/Timer.h>
#include <System/Context.h>

#include "TransactionApiHelpers.h"

using namespace Crypto;
using namespace Common;
using namespace CryptoNote;

namespace CryptoNote {
    std::ostream& operator<<(std::ostream& o, const WalletTransactionState& st) {
      switch (st) {
        case WalletTransactionState::FAILED:
          o << "FAILED";
          break;
        case WalletTransactionState::CANCELLED:
          o << "CANCELLED";
          break;
        case WalletTransactionState::SUCCEEDED:
          o << "SUCCEEDED";
          break;
      }
      return o;
    }
    std::ostream& operator<<(std::ostream& o, const WalletTransaction& tx) {
      o << "WalletTransaction{state=" << tx.state << ", timestamp=" << tx.timestamp
        << ", blockHeight=" << tx.blockHeight << ", hash=" << tx.hash
        << ", totalAmount=" << tx.totalAmount << ", fee=" << tx.fee
        << ", creationTime=" << tx.creationTime << ", unlockTime=" << tx.unlockTime
        << ", extra=" << tx.extra  << ", isBase=" << tx.isBase << "}";
      return o;
    }

    bool operator==(const WalletTransaction& lhs, const WalletTransaction& rhs) {
      if (lhs.state != rhs.state) {
        return false;
      }

      if (lhs.timestamp != rhs.timestamp) {
        return false;
      }

      if (lhs.blockHeight != rhs.blockHeight) {
        return false;
      }

      if (lhs.hash != rhs.hash) {
        return false;
      }

      if (lhs.totalAmount != rhs.totalAmount) {
        return false;
      }

      if (lhs.fee != rhs.fee) {
        return false;
      }

      if (lhs.creationTime != rhs.creationTime) {
        return false;
      }

      if (lhs.unlockTime != rhs.unlockTime) {
        return false;
      }

      if (lhs.extra != rhs.extra) {
        return false;
      }

      if (lhs.isBase != rhs.isBase) {
        return false;
      }

      return true;
    }

    bool operator!=(const WalletTransaction& lhs, const WalletTransaction& rhs) {
      return !(lhs == rhs);
    }

    bool operator==(const WalletTransfer& lhs, const WalletTransfer& rhs) {
      if (lhs.address != rhs.address) {
        return false;
      }

      if (lhs.amount != rhs.amount) {
        return false;
      }

      return true;
    }

    bool operator!=(const WalletTransfer& lhs, const WalletTransfer& rhs) {
      return !(lhs == rhs);
    }

    bool operator==(const IFusionManager::EstimateResult& lhs, const IFusionManager::EstimateResult& rhs) {
      return lhs.fusionReadyCount == rhs.fusionReadyCount && lhs.totalOutputCount == rhs.totalOutputCount;
    }
}

class WalletApi: public ::testing::Test {
public:
  WalletApi() :
    TRANSACTION_SOFTLOCK_TIME(10),
    currency(CryptoNote::CurrencyBuilder(logger).currency()),
    generator(currency),
    node(generator),
    alice(dispatcher, currency, node),
    FEE(currency.minimumFee()),
    FUSION_THRESHOLD(currency.defaultDustThreshold() * 10)
  { }

  virtual void SetUp() override;
  virtual void TearDown() override;

protected:
  CryptoNote::AccountPublicAddress parseAddress(const std::string& address);
  void generateBlockReward();
  void generateBlockReward(const std::string& address);
  void generateAndUnlockMoney();
  void generateAddressesWithPendingMoney(size_t count);
  void generateFusionOutputsAndUnlock(WalletGreen& wallet, INodeTrivialRefreshStub& node, const CryptoNote::Currency& walletCurrency, uint64_t threshold);
  void unlockMoney();
  void unlockMoney(CryptoNote::WalletGreen& wallet, INodeTrivialRefreshStub& inode);
  void setMinerTo(CryptoNote::WalletGreen& wallet);

  template<typename T>
  void waitValueChanged(CryptoNote::WalletGreen& wallet, T prev, std::function<T ()>&& f);

  template<typename T>
  void waitForValue(CryptoNote::WalletGreen& wallet, T value, std::function<T ()>&& f);

  bool waitForWalletEvent(CryptoNote::WalletGreen& wallet, CryptoNote::WalletEventType eventType, std::chrono::nanoseconds timeout);

  void waitActualBalanceUpdated();
  void waitActualBalanceUpdated(uint64_t prev);
  void waitActualBalanceUpdated(CryptoNote::WalletGreen& wallet, uint64_t prev);

  void waitPendingBalanceUpdated();
  void waitPendingBalanceUpdated(uint64_t prev);
  void waitPendingBalanceUpdated(CryptoNote::WalletGreen& wallet, uint64_t prev);

  void waitForTransactionCount(CryptoNote::WalletGreen& wallet, uint64_t expected);
  void waitForTransactionUpdated(CryptoNote::WalletGreen& wallet, size_t expectedTransactionId);
  void waitForActualBalance(uint64_t expected);
  void waitForActualBalance(CryptoNote::WalletGreen& wallet, uint64_t expected);

  size_t sendMoneyToRandomAddressFrom(const std::string& address, uint64_t amount, uint64_t fee);
  size_t sendMoneyToRandomAddressFrom(const std::string& address);

  size_t sendMoney(CryptoNote::WalletGreen& wallet, const std::string& to, int64_t amount, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0);
  size_t sendMoney(const std::string& to, int64_t amount, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0);

  void fillWalletWithDetailsCache();

  void wait(uint64_t milliseconds);
  void testIWalletDataCompatibility(bool details, const std::string& cache = std::string(),
          const std::vector<WalletLegacyTransaction>& txs = std::vector<WalletLegacyTransaction>(),
          const std::vector<WalletLegacyTransfer>& trs = std::vector<WalletLegacyTransfer>(),
          const std::vector<std::pair<TransactionInformation, int64_t>>& externalTxs = std::vector<std::pair<TransactionInformation, int64_t>>());

  uint32_t TRANSACTION_SOFTLOCK_TIME;

  System::Dispatcher dispatcher;
  Logging::ConsoleLogger logger;
  CryptoNote::Currency currency;
  TestBlockchainGenerator generator;
  INodeTrivialRefreshStub node;
  CryptoNote::WalletGreen alice;
  std::string aliceAddress;

  const uint64_t SENT = 1122334455;
  const uint64_t FEE;
  const std::string RANDOM_ADDRESS = "2634US2FAz86jZT73YmM8u5GPCknT2Wxj8bUCKivYKpThFhF2xsjygMGxbxZzM42zXhKUhym6Yy6qHHgkuWtruqiGkDpX6m";
  const uint64_t FUSION_THRESHOLD;
};

void WalletApi::SetUp() {
  alice.initialize("pass");
  aliceAddress = alice.createAddress();
}

void WalletApi::setMinerTo(CryptoNote::WalletGreen& wallet) {
  AccountBase base;
  AccountKeys keys;
  auto viewKey = wallet.getViewKey();
  auto spendKey = wallet.getAddressSpendKey(0);
  keys.address.spendPublicKey = spendKey.publicKey;
  keys.address.viewPublicKey = viewKey.publicKey;
  keys.viewSecretKey = viewKey.secretKey;
  keys.spendSecretKey = spendKey.secretKey;
  base.setAccountKeys(keys);
  // mine to alice's address to make it recieve block base transaction
  generator.setMinerAccount(base);
}

void WalletApi::TearDown() {
  alice.shutdown();
  wait(100); //ObserverManager bug workaround
}

CryptoNote::AccountPublicAddress WalletApi::parseAddress(const std::string& address) {
  CryptoNote::AccountPublicAddress pubAddr;
  if (!currency.parseAccountAddressString(address, pubAddr)) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }

  return pubAddr;
}

void WalletApi::generateBlockReward() {
  generateBlockReward(aliceAddress);
}

void WalletApi::generateBlockReward(const std::string& address) {
  generator.getBlockRewardForAddress(parseAddress(address));
}

void WalletApi::generateFusionOutputsAndUnlock(WalletGreen& wallet, INodeTrivialRefreshStub& node, const CryptoNote::Currency& walletCurrency, uint64_t threshold) {
  uint64_t digit = walletCurrency.defaultDustThreshold();
  uint64_t mul = 1;

  while (digit > 9) {
    digit /= 10;
    mul *= 10;
  }

  auto initialAmount = wallet.getActualBalance();

  CryptoNote::AccountPublicAddress publicAddress = parseAddress(wallet.getAddress(0));
  const size_t POWERS_COUNT = 3;

  uint64_t addedAmount = 0;
  for (size_t power = 0; power < POWERS_COUNT; ++power) {
    int start = power == 0 ? digit: 1;
    if (start * mul > threshold) {
      break;
    }

    for (int count = 0, d = start; count < walletCurrency.fusionTxMinInputCount() && start * mul < threshold; ++count) {
      //TODO: make it possible to put several outputs to one transaction
      auto amount = d * mul;
      generator.getSingleOutputTransaction(publicAddress, amount);
      addedAmount += amount;

      if (++d > 9 || amount >= threshold) {
        d = start;
      }
    }

    mul *= 10;
  }

  assert(addedAmount > 0);

  generator.generateEmptyBlocks(11);
  node.updateObservers();

  waitForActualBalance(wallet, initialAmount + addedAmount);
}

void WalletApi::unlockMoney() {
  unlockMoney(alice, node);
}

void WalletApi::unlockMoney(CryptoNote::WalletGreen& wallet, INodeTrivialRefreshStub& inode) {
  auto prev = wallet.getActualBalance();
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow()); //coinbase money should become available after 10 blocks
  inode.updateObservers();
  waitActualBalanceUpdated(wallet, prev);
}

void WalletApi::generateAndUnlockMoney() {
  generateBlockReward();
  unlockMoney();
}

template<typename T>
void WalletApi::waitValueChanged(CryptoNote::WalletGreen& wallet, T prev, std::function<T ()>&& f) {
  while (prev == f()) {
    wallet.getEvent();
  }
}

template<typename T>
void WalletApi::waitForValue(CryptoNote::WalletGreen& wallet, T value, std::function<T ()>&& f) {
  while (value != f()) {
    wallet.getEvent();
  }
}

bool WalletApi::waitForWalletEvent(CryptoNote::WalletGreen& wallet, CryptoNote::WalletEventType eventType, std::chrono::nanoseconds timeout) {
  System::Context<> eventContext(dispatcher, [&wallet, eventType] () {
    CryptoNote::WalletEvent event;

    do {
      event = wallet.getEvent();
    } while(event.type != eventType);
  });

  System::Context<> timeoutContext(dispatcher, [timeout, &eventContext, this] {
    System::Timer(dispatcher).sleep(timeout);
    eventContext.interrupt();
  });

  try {
    eventContext.get();
    return true;
  } catch (System::InterruptedException&) {
    return false;
  }
}

void WalletApi::waitActualBalanceUpdated() {
  waitActualBalanceUpdated(alice, alice.getActualBalance());
}

void WalletApi::waitActualBalanceUpdated(uint64_t prev) {
  waitActualBalanceUpdated(alice, prev);
}

void WalletApi::waitForActualBalance(uint64_t expected) {
  waitForValue<uint64_t>(alice, expected, [this] () { return this->alice.getActualBalance(); });
}

void WalletApi::waitForActualBalance(CryptoNote::WalletGreen& wallet, uint64_t expected) {
  waitForValue<uint64_t>(wallet, expected, [&wallet] () { return wallet.getActualBalance(); });
}

void WalletApi::waitActualBalanceUpdated(CryptoNote::WalletGreen& wallet, uint64_t prev) {
  waitValueChanged<uint64_t>(wallet, prev, [&wallet] () { return wallet.getActualBalance(); });
}

void WalletApi::waitPendingBalanceUpdated() {
  waitPendingBalanceUpdated(alice, alice.getPendingBalance());
}

void WalletApi::waitPendingBalanceUpdated(uint64_t prev) {
  waitPendingBalanceUpdated(alice, prev);
}

void WalletApi::waitPendingBalanceUpdated(CryptoNote::WalletGreen& wallet, uint64_t prev) {
  waitValueChanged<uint64_t>(wallet, prev, [&wallet] () { return wallet.getPendingBalance(); });
}

void WalletApi::waitForTransactionCount(CryptoNote::WalletGreen& wallet, uint64_t expected) {
  waitForValue<size_t>(wallet, expected, [&wallet] () { return wallet.getTransactionCount(); });
}

void WalletApi::waitForTransactionUpdated(CryptoNote::WalletGreen& wallet, size_t expectedTransactionId) {
  WalletEvent event;
  for (;;) {
    event = wallet.getEvent();
    if (event.type == WalletEventType::TRANSACTION_UPDATED && event.transactionUpdated.transactionIndex == expectedTransactionId) {
      break;
    }
  }
}

void WalletApi::generateAddressesWithPendingMoney(size_t count) {
  for (size_t i = 0; i < count; ++i) {
    generateBlockReward(alice.createAddress());
  }
}

size_t WalletApi::sendMoneyToRandomAddressFrom(const std::string& address, uint64_t amount, uint64_t fee) {
  CryptoNote::WalletTransfer transfer;
  transfer.address = RANDOM_ADDRESS;
  transfer.amount = amount;

  return alice.transfer(address, transfer, fee, 0);
}

size_t WalletApi::sendMoneyToRandomAddressFrom(const std::string& address) {
  return sendMoneyToRandomAddressFrom(address, SENT, FEE);
}

void WalletApi::fillWalletWithDetailsCache() {
  generateAddressesWithPendingMoney(10);
  unlockMoney();

  auto alicePrev = alice.getActualBalance();
  for (size_t i = 1; i < 5; ++i) {
    sendMoneyToRandomAddressFrom(alice.getAddress(i));
  }

  node.updateObservers();
  waitActualBalanceUpdated(alicePrev);

  for (size_t i = 5; i < 10; ++i) {
    sendMoneyToRandomAddressFrom(alice.getAddress(i));
  }
}

size_t WalletApi::sendMoney(CryptoNote::WalletGreen& wallet, const std::string& to, int64_t amount, uint64_t fee, uint64_t mixIn, const std::string& extra, uint64_t unlockTimestamp) {
  CryptoNote::WalletTransfer transfer;
  transfer.address = to;
  transfer.amount = amount;

  return wallet.transfer(transfer, fee, mixIn, extra, unlockTimestamp);
}

size_t WalletApi::sendMoney(const std::string& to, int64_t amount, uint64_t fee, uint64_t mixIn, const std::string& extra, uint64_t unlockTimestamp) {
  return sendMoney(alice, to, amount, fee, mixIn, extra, unlockTimestamp);
}

void WalletApi::wait(uint64_t milliseconds) {
  System::Timer timer(dispatcher);
  timer.sleep(std::chrono::nanoseconds(milliseconds * 1000000));
}

static const uint64_t TEST_BLOCK_REWARD = 70368744177663;

TEST_F(WalletApi, emptyBalance) {
  ASSERT_EQ(0, alice.getActualBalance());
  ASSERT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, receiveMoneyOneAddress) {
  generateBlockReward();

  auto prev = alice.getPendingBalance();
  node.updateObservers();
  waitPendingBalanceUpdated(prev);

  ASSERT_EQ(0, alice.getActualBalance());
  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getPendingBalance());

  ASSERT_EQ(0, alice.getActualBalance(aliceAddress));
  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getPendingBalance(aliceAddress));
}

TEST_F(WalletApi, unlockMoney) {
  generateAndUnlockMoney();

  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getActualBalance());
  ASSERT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, transferFromOneAddress) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize("pass2");
  std::string bobAddress = bob.createAddress();

  generateAndUnlockMoney();

  auto alicePrev = alice.getActualBalance();
  sendMoney(bobAddress, SENT, FEE);
  node.updateObservers();

  waitActualBalanceUpdated(alicePrev);
  waitPendingBalanceUpdated(bob, 0);

  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(SENT, bob.getPendingBalance());

  ASSERT_EQ(TEST_BLOCK_REWARD - SENT - FEE, alice.getActualBalance() + alice.getPendingBalance());
  ASSERT_EQ(TEST_BLOCK_REWARD - SENT - FEE, alice.getActualBalance(aliceAddress) + alice.getPendingBalance(aliceAddress));

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, pendingBalanceUpdatedAfterTransactionGotInBlock) {
  generateAndUnlockMoney();

  auto initialActual = alice.getActualBalance();

  sendMoney(RANDOM_ADDRESS, SENT, FEE);
  node.updateObservers();
  waitActualBalanceUpdated(initialActual);
  waitPendingBalanceUpdated(0);

  auto prevPending = alice.getPendingBalance();

  generator.generateEmptyBlocks(TRANSACTION_SOFTLOCK_TIME);
  node.updateObservers();

  waitPendingBalanceUpdated(prevPending);
  ASSERT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, moneyLockedIfTransactionIsSoftLocked) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize("pass2");

  sendMoney(bob.createAddress(), SENT, FEE);
  generator.generateEmptyBlocks(TRANSACTION_SOFTLOCK_TIME - 1);
  node.updateObservers();

  waitPendingBalanceUpdated(bob, 0);

  ASSERT_EQ(SENT, bob.getPendingBalance());
  ASSERT_EQ(0, bob.getActualBalance());

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, transferMixin) {
  generateAndUnlockMoney();

  auto alicePrev = alice.getActualBalance();

  ASSERT_NO_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE, 12));
  node.updateObservers();

  waitActualBalanceUpdated(alicePrev);

  auto tx = alice.getTransaction(0);
  ASSERT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, tx.state);
}

TEST_F(WalletApi, transferTooBigMixin) {
  generateAndUnlockMoney();

  node.setMaxMixinCount(10);
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE, 15));
}

TEST_F(WalletApi, transferNegativeAmount) {
  generateAndUnlockMoney();
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, -static_cast<int64_t>(SENT), FEE));
}

TEST_F(WalletApi, transferFromTwoAddresses) {
  generateBlockReward();
  generateBlockReward(alice.createAddress());
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  node.updateObservers();

  waitForActualBalance(2 * TEST_BLOCK_REWARD);

  CryptoNote::WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize("pass2");
  std::string bobAddress = bob.createAddress();

  const uint64_t sent = 2 * TEST_BLOCK_REWARD - 10 * FEE;

  auto bobPrev = bob.getPendingBalance();
  auto alicePendingPrev = alice.getPendingBalance();
  auto aliceActualPrev = alice.getActualBalance();

  sendMoney(bobAddress, sent, FEE);

  node.updateObservers();

  waitActualBalanceUpdated(aliceActualPrev);
  waitPendingBalanceUpdated(bob, bobPrev);
  waitPendingBalanceUpdated(alicePendingPrev);

  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(sent, bob.getPendingBalance());

  ASSERT_EQ(2 * TEST_BLOCK_REWARD - sent - FEE, alice.getActualBalance() + alice.getPendingBalance());

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, transferTooBigTransaction) {
  CryptoNote::Currency cur = CryptoNote::CurrencyBuilder(logger).blockGrantedFullRewardZone(5).minerTxBlobReservedSize(2).currency();
  TestBlockchainGenerator gen(cur);
  INodeTrivialRefreshStub n(gen);

  CryptoNote::WalletGreen wallet(dispatcher, cur, n, TRANSACTION_SOFTLOCK_TIME);
  wallet.initialize("pass");
  wallet.createAddress();

  gen.getBlockRewardForAddress(parseAddress(wallet.getAddress(0)));

  auto prev = wallet.getActualBalance();
  gen.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  n.updateObservers();
  waitActualBalanceUpdated(wallet, prev);

  CryptoNote::WalletTransfer transfer;
  transfer.address = RANDOM_ADDRESS;
  transfer.amount = SENT;

  ASSERT_ANY_THROW(wallet.transfer(transfer, FEE));
}

TEST_F(WalletApi, balanceAfterTransfer) {
  generateAndUnlockMoney();

  sendMoney(RANDOM_ADDRESS, SENT, FEE);

  ASSERT_EQ(TEST_BLOCK_REWARD - SENT - FEE, alice.getActualBalance() + alice.getPendingBalance());
}

TEST_F(WalletApi, specificAddressesBalances) {
  generateAndUnlockMoney();

  auto secondAddress = alice.createAddress();
  generateBlockReward(secondAddress);
  node.updateObservers();
  waitPendingBalanceUpdated();

  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getActualBalance());
  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getActualBalance(aliceAddress));
  ASSERT_EQ(0, alice.getActualBalance(secondAddress));

  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getPendingBalance());
  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getPendingBalance(secondAddress));
  ASSERT_EQ(0, alice.getPendingBalance(aliceAddress));
}

TEST_F(WalletApi, transferFromSpecificAddress) {
  generateBlockReward();

  auto secondAddress = alice.createAddress();
  generateBlockReward(secondAddress);

  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  node.updateObservers();
  waitActualBalanceUpdated();

  auto prevActual = alice.getActualBalance();
  auto prevPending = alice.getPendingBalance();

  sendMoneyToRandomAddressFrom(secondAddress);

  node.updateObservers();
  waitActualBalanceUpdated(prevActual);
  waitPendingBalanceUpdated(prevPending);

  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getActualBalance(aliceAddress));

  //NOTE: do not expect the rule 'actual + pending == previous - sent - fee' to work,
  //because change is sent to address #0.
  ASSERT_NE(TEST_BLOCK_REWARD, alice.getActualBalance(secondAddress));
  ASSERT_NE(0, alice.getPendingBalance(aliceAddress));
  ASSERT_EQ(2 * TEST_BLOCK_REWARD - SENT - FEE, alice.getActualBalance() + alice.getPendingBalance());
}

TEST_F(WalletApi, loadEmptyWallet) {
  std::stringstream data;
  alice.save(data, true, true);

  WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.load(data, "pass");

  ASSERT_EQ(alice.getAddressCount(), bob.getAddressCount());
  ASSERT_EQ(alice.getActualBalance(), bob.getActualBalance());
  ASSERT_EQ(alice.getPendingBalance(), bob.getPendingBalance());
  ASSERT_EQ(alice.getTransactionCount(), bob.getTransactionCount());

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, walletGetsBaseTransaction) {
  // mine to alice's address to make it recieve block base transaction
  setMinerTo(alice);
  generateAndUnlockMoney();
  ASSERT_TRUE(alice.getTransaction(0).isBase);
}

TEST_F(WalletApi, walletGetsNonBaseTransaction) {
  generateAndUnlockMoney();
  ASSERT_FALSE(alice.getTransaction(0).isBase);
}

TEST_F(WalletApi, loadWalletWithBaseTransaction) {
  // mine to alice's address to make it recieve block base transaction
  setMinerTo(alice);
  generateAndUnlockMoney();

  std::stringstream data;
  alice.save(data, true, true);

  WalletGreen bob(dispatcher, currency, node);
  bob.load(data, "pass");

  ASSERT_TRUE(bob.getTransaction(0).isBase);
  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, updateBaseTransactionAfterLoad) {
  // mine to alice's address to make it recieve block base transaction
  setMinerTo(alice);
  generateAndUnlockMoney();

  std::stringstream data;
  alice.save(data, true, false);

  WalletGreen bob(dispatcher, currency, node);
  bob.load(data, "pass");
  waitForWalletEvent(bob, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));

  ASSERT_TRUE(bob.getTransaction(0).isBase);
  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, setBaseTransactionAfterInSynchronization) {
  // mine to alice's address to make it recieve block base transaction
  setMinerTo(alice);
  generateAndUnlockMoney();

  std::stringstream data;
  alice.save(data, false, false);

  WalletGreen bob(dispatcher, currency, node);
  bob.load(data, "pass");
  waitForWalletEvent(bob, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));

  ASSERT_TRUE(bob.getTransaction(0).isBase);
  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, loadWalletWithoutAddresses) {
  WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize("pass");

  std::stringstream data;
  bob.save(data, false, false);
  bob.shutdown();

  WalletGreen carol(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  carol.load(data, "pass");

  ASSERT_EQ(0, carol.getAddressCount());
  carol.shutdown();
  wait(100);
}

void compareWalletsAddresses(const CryptoNote::WalletGreen& alice, const CryptoNote::WalletGreen& bob) {
  ASSERT_EQ(alice.getAddressCount(), bob.getAddressCount());
  for (size_t i = 0; i < alice.getAddressCount(); ++i) {
    ASSERT_EQ(alice.getAddress(i), bob.getAddress(i));
  }
}

void compareWalletsActualBalance(const CryptoNote::WalletGreen& alice, const CryptoNote::WalletGreen& bob) {
  ASSERT_EQ(alice.getActualBalance(), bob.getActualBalance());
  for (size_t i = 0; i < bob.getAddressCount(); ++i) {
    auto addr = bob.getAddress(i);
    ASSERT_EQ(alice.getActualBalance(addr), bob.getActualBalance(addr));
  }
}

void compareWalletsPendingBalance(const CryptoNote::WalletGreen& alice, const CryptoNote::WalletGreen& bob) {
  ASSERT_EQ(alice.getPendingBalance(), bob.getPendingBalance());
  for (size_t i = 0; i < bob.getAddressCount(); ++i) {
    auto addr = bob.getAddress(i);
    ASSERT_EQ(alice.getActualBalance(addr), bob.getActualBalance(addr));
  }
}

void compareWalletsTransactionTransfers(const CryptoNote::WalletGreen& alice, const CryptoNote::WalletGreen& bob) {
  ASSERT_EQ(alice.getTransactionCount(), bob.getTransactionCount());
  for (size_t i = 0; i < bob.getTransactionCount(); ++i) {
    ASSERT_EQ(alice.getTransaction(i), bob.getTransaction(i));

    ASSERT_EQ(alice.getTransactionTransferCount(i), bob.getTransactionTransferCount(i));

    size_t trCount = bob.getTransactionTransferCount(i);
    for (size_t j = 0; j < trCount; ++j) {
      ASSERT_EQ(alice.getTransactionTransfer(i, j), bob.getTransactionTransfer(i, j));
    }
  }
}

TEST_F(WalletApi, loadCacheDetails) {
  fillWalletWithDetailsCache();
  node.waitForAsyncContexts();
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));
  std::stringstream data;
  alice.save(data, true, true);

  WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.load(data, "pass");

  compareWalletsAddresses(alice, bob);
  compareWalletsActualBalance(alice, bob);
  compareWalletsPendingBalance(alice, bob);
  compareWalletsTransactionTransfers(alice, bob);

  bob.shutdown();
  wait(100); //ObserverManager bug workaround
}

TEST_F(WalletApi, loadNoCacheNoDetails) {
  fillWalletWithDetailsCache();

  std::stringstream data;
  alice.save(data, false, false);

  WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.load(data, "pass");

  compareWalletsAddresses(alice, bob);

  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(0, bob.getPendingBalance());
  ASSERT_EQ(0, bob.getTransactionCount());

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, loadNoCacheDetails) {
  fillWalletWithDetailsCache();

  std::stringstream data;
  alice.save(data, true, false);

  WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.load(data, "pass");

  compareWalletsAddresses(alice, bob);

  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(0, bob.getPendingBalance());

  compareWalletsTransactionTransfers(alice, bob);

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, loadCacheNoDetails) {
  fillWalletWithDetailsCache();

  std::stringstream data;
  alice.save(data, false, true);

  WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.load(data, "pass");

  compareWalletsAddresses(alice, bob);
  compareWalletsActualBalance(alice, bob);
  compareWalletsPendingBalance(alice, bob);

  ASSERT_EQ(0, bob.getTransactionCount());

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, loadWithWrongPassword) {
  std::stringstream data;
  alice.save(data, false, false);

  WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_ANY_THROW(bob.load(data, "pass2"));
}

void WalletApi::testIWalletDataCompatibility(bool details, const std::string& cache, const std::vector<WalletLegacyTransaction>& txs,
    const std::vector<WalletLegacyTransfer>& trs, const std::vector<std::pair<TransactionInformation, int64_t>>& externalTxs) {
  CryptoNote::AccountBase account;
  account.generate();

  WalletUserTransactionsCache iWalletCache;
  WalletLegacySerializer walletSerializer(account, iWalletCache);

  for (const auto& tx: txs) {
    std::vector<WalletLegacyTransfer> txtrs;
    if (tx.firstTransferId != WALLET_LEGACY_INVALID_TRANSFER_ID && tx.transferCount != 0) {
      for (size_t i = tx.firstTransferId; i < (tx.firstTransferId  + tx.transferCount); ++i) {
        txtrs.push_back(trs[i]);
      }
    }
    auto txId = iWalletCache.addNewTransaction(tx.totalAmount, tx.fee, tx.extra, txtrs, tx.unlockTime);
    iWalletCache.updateTransactionSendingState(txId, std::error_code());
  }

  for (const auto& item: externalTxs) {
    iWalletCache.onTransactionUpdated(item.first, item.second);
  }

  std::stringstream stream;
  walletSerializer.serialize(stream, "pass", details, std::string());

  WalletGreen wallet(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  wallet.load(stream, "pass");

  EXPECT_EQ(1, wallet.getAddressCount());

  AccountPublicAddress addr;
  currency.parseAccountAddressString(wallet.getAddress(0), addr);
  EXPECT_EQ(account.getAccountKeys().address.spendPublicKey, addr.spendPublicKey);
  EXPECT_EQ(account.getAccountKeys().address.viewPublicKey, addr.viewPublicKey);
  EXPECT_EQ(0, wallet.getActualBalance());
  EXPECT_EQ(0, wallet.getPendingBalance());

  if (details) {
    auto outcomingTxCount = wallet.getTransactionCount() - externalTxs.size();
    ASSERT_EQ(txs.size(), outcomingTxCount);
    for (size_t i = 0; i < outcomingTxCount; ++i) {
      auto tx = wallet.getTransaction(i);
      EXPECT_EQ(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, tx.blockHeight);
      EXPECT_EQ(txs[i].extra, tx.extra);
      EXPECT_EQ(txs[i].fee, tx.fee);
      EXPECT_EQ(WalletTransactionState::SUCCEEDED, tx.state);
      EXPECT_EQ(-txs[i].totalAmount, tx.totalAmount);
      EXPECT_EQ(txs[i].unlockTime, tx.unlockTime);

      size_t trsCount = wallet.getTransactionTransferCount(i);
      ASSERT_EQ(txs[i].transferCount, trsCount);
      for (size_t j = 0; j < trsCount; ++j) {
        ASSERT_NE(WALLET_LEGACY_INVALID_TRANSFER_ID, txs[i].firstTransferId);

        size_t index = txs[i].firstTransferId + j;
        EXPECT_EQ(trs[index].address, wallet.getTransactionTransfer(i, j).address);
        EXPECT_EQ(trs[index].amount, wallet.getTransactionTransfer(i, j).amount);
      }
    }

    ASSERT_EQ(txs.size() + externalTxs.size(), wallet.getTransactionCount());
    for (size_t i = outcomingTxCount; i < wallet.getTransactionCount(); ++i) {
      auto inTx = externalTxs[i - outcomingTxCount].first;
      auto txBalance = externalTxs[i - outcomingTxCount].second;
      auto tx = wallet.getTransaction(i);

      EXPECT_EQ(inTx.blockHeight, tx.blockHeight);
      EXPECT_EQ(0, tx.creationTime);
      std::string extraString(inTx.extra.begin(), inTx.extra.end());
      EXPECT_EQ(extraString, tx.extra);
      EXPECT_EQ(txBalance, tx.totalAmount);

      if (inTx.totalAmountIn) {
        EXPECT_EQ(inTx.totalAmountIn - inTx.totalAmountOut, tx.fee);
      } else {
        EXPECT_EQ(0, tx.fee);
      }

      EXPECT_EQ(inTx.transactionHash, tx.hash);
      EXPECT_EQ(WalletTransactionState::SUCCEEDED, tx.state);
      EXPECT_EQ(inTx.unlockTime, tx.unlockTime);
    }
  } else {
    EXPECT_EQ(0, wallet.getTransactionCount());
  }
}

TEST_F(WalletApi, IWalletDataCompatibilityEmptyDetailsNoCache) {
  testIWalletDataCompatibility(true);
}

TEST_F(WalletApi, IWalletDataCompatibilityEmptyNoDetailsNoCache) {
  testIWalletDataCompatibility(false);
}

TEST_F(WalletApi, IWalletDataCompatibilityEmptyNoDetailsCache) {
  std::string cache(1024, 'c');
  testIWalletDataCompatibility(false, cache);
}

TEST_F(WalletApi, IWalletDataCompatibilityEmptyDetailsCache) {
  std::string cache(1024, 'c');
  testIWalletDataCompatibility(true, cache);
}

TEST_F(WalletApi, IWalletDataCompatibilityDetails) {
  std::vector<WalletLegacyTransaction> txs;

  WalletLegacyTransaction tx1;
  tx1.firstTransferId = 0;
  tx1.transferCount = 2;
  tx1.unlockTime = 12;
  tx1.totalAmount = 1234567890;
  tx1.timestamp = (uint64_t) 8899007711;
  tx1.extra = "jsjeokvsnxcvkhdoifjaslkcvnvuergeonlsdnlaksmdclkasowehunkjn";
  tx1.fee = 1000;
  tx1.isCoinbase = false;
  txs.push_back(tx1);

  std::vector<WalletLegacyTransfer> trs;

  WalletLegacyTransfer tr1;
  tr1.address = RANDOM_ADDRESS;
  tr1.amount = SENT;
  trs.push_back(tr1);

  WalletLegacyTransfer tr2;
  tr2.amount = 102034;
  tr2.address = alice.getAddress(0);
  trs.push_back(tr2);

  std::vector<std::pair<TransactionInformation, int64_t>> incomingTxs;

  TransactionInformation iTx1;
  iTx1.timestamp = 929453;
  iTx1.totalAmountIn = 200353;
  iTx1.blockHeight = 2349;
  std::iota(iTx1.transactionHash.data, iTx1.transactionHash.data+32, 125);
  iTx1.extra = {1,2,3,4,5,6,7,8,9,1,2,3,4,5,6,7,8,9};
  std::iota(iTx1.publicKey.data, iTx1.publicKey.data+32, 15);
  iTx1.totalAmountOut = 948578;
  iTx1.unlockTime = 17;
  incomingTxs.push_back(std::make_pair(iTx1, 99874442));

  TransactionInformation iTx2;
  iTx2.timestamp = 10010;
  iTx2.totalAmountIn = 0;
  iTx2.blockHeight = 2350;
  std::iota(iTx2.transactionHash.data, iTx2.transactionHash.data+32, 15);
  iTx2.extra = {11,22,33,44,55,66,77,88,99,12,13,14,15,16};
  std::iota(iTx2.publicKey.data, iTx2.publicKey.data+32, 5);
  iTx2.totalAmountOut = 99874442;
  iTx2.unlockTime = 12;
  incomingTxs.push_back(std::make_pair(iTx2, 99874442));

  std::string cache(1024, 'c');
  testIWalletDataCompatibility(true, cache, txs, trs, incomingTxs);
}

TEST_F(WalletApi, getEventStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.getEvent());
}

TEST_F(WalletApi, stopStart) {
  alice.stop();
  alice.start();

  ASSERT_NO_THROW(alice.getActualBalance());
}

TEST_F(WalletApi, uninitializedObject) {
  WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);

  ASSERT_ANY_THROW(bob.changePassword("s", "p"));
  std::stringstream stream;
  ASSERT_ANY_THROW(bob.save(stream));
  ASSERT_ANY_THROW(bob.getAddressCount());
  ASSERT_ANY_THROW(bob.getAddress(0));
  ASSERT_ANY_THROW(bob.createAddress());
  ASSERT_ANY_THROW(bob.deleteAddress(RANDOM_ADDRESS));
  ASSERT_ANY_THROW(bob.getActualBalance());
  ASSERT_ANY_THROW(bob.getActualBalance(RANDOM_ADDRESS));
  ASSERT_ANY_THROW(bob.getPendingBalance());
  ASSERT_ANY_THROW(bob.getPendingBalance(RANDOM_ADDRESS));
  ASSERT_ANY_THROW(bob.getTransactionCount());
  ASSERT_ANY_THROW(bob.getTransaction(0));
  ASSERT_ANY_THROW(bob.getTransactionTransferCount(0));
  ASSERT_ANY_THROW(bob.getTransactionTransfer(0, 0));
  ASSERT_ANY_THROW(sendMoneyToRandomAddressFrom(aliceAddress));
  ASSERT_ANY_THROW(bob.shutdown());
  wait(100);
}

const size_t TX_PUB_KEY_EXTRA_SIZE = 33;

TEST_F(WalletApi, checkSentTransaction) {
  generateAndUnlockMoney();
  size_t txId = sendMoney(RANDOM_ADDRESS, SENT, FEE);

  CryptoNote::WalletTransaction tx = alice.getTransaction(txId);
  ASSERT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, tx.state);
  ASSERT_EQ(0, tx.timestamp);
  ASSERT_EQ(CryptoNote::WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, tx.blockHeight);
  ASSERT_EQ(-static_cast<int64_t>(SENT + FEE), tx.totalAmount);
  ASSERT_EQ(FEE, tx.fee);
  ASSERT_EQ(0, tx.unlockTime);
  ASSERT_FALSE(tx.isBase);
  ASSERT_EQ(TX_PUB_KEY_EXTRA_SIZE, tx.extra.size()); //Transaction public key only
}

std::string removeTxPublicKey(const std::string& txExtra) {
  if (txExtra.size() <= TX_PUB_KEY_EXTRA_SIZE) {
    return std::string();
  }

  return txExtra.substr(TX_PUB_KEY_EXTRA_SIZE);
}

std::string createExtraNonce(const std::string& nonce) {
  CryptoNote::TransactionExtra txExtra;
  CryptoNote::TransactionExtraNonce extraNonce;
  extraNonce.nonce = asBinaryArray(nonce);
  txExtra.set(extraNonce);
  auto vec = txExtra.serialize();
  return std::string(vec.begin(), vec.end());
}

TEST_F(WalletApi, checkSentTransactionWithExtra) {
  const std::string extra = createExtraNonce("\x01\x23\x45\x67\x89\xab\xcd\xef");

  generateAndUnlockMoney();
  size_t txId = sendMoney(RANDOM_ADDRESS, SENT, FEE, 0, extra);

  CryptoNote::WalletTransaction tx = alice.getTransaction(txId);
  ASSERT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, tx.state);
  ASSERT_EQ(0, tx.timestamp);
  ASSERT_EQ(CryptoNote::WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, tx.blockHeight);
  ASSERT_EQ(-static_cast<int64_t>(SENT + FEE), tx.totalAmount);
  ASSERT_EQ(FEE, tx.fee);
  ASSERT_EQ(0, tx.unlockTime);
  ASSERT_FALSE(tx.isBase);
  ASSERT_EQ(extra, removeTxPublicKey(tx.extra));
}

TEST_F(WalletApi, checkFailedTransaction) {
  generateAndUnlockMoney();

  node.setNextTransactionError();
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE));

  auto tx = alice.getTransaction(alice.getTransactionCount() - 1);
  ASSERT_EQ(CryptoNote::WalletTransactionState::FAILED, tx.state);
}

TEST_F(WalletApi, transactionSendsAfterFailedTransaction) {
  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE);
  unlockMoney();

  node.setNextTransactionError();
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE));
  ASSERT_NO_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE));
}

TEST_F(WalletApi, checkIncomingTransaction) {
  const std::string extra = createExtraNonce("\x01\x23\x45\x67\x89\xab\xcd\xef");

  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize("pass2");
  std::string bobAddress = bob.createAddress();

  sendMoney(bobAddress, SENT, FEE, 0, extra, 11);
  node.updateObservers();
  waitPendingBalanceUpdated(bob, 0);

  auto tx = bob.getTransaction(bob.getTransactionCount() - 1);

  bob.shutdown();
  wait(100); //observer manager bug

  ASSERT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, tx.state);
  ASSERT_NE(0, tx.timestamp);
  ASSERT_EQ(generator.getBlockchain().size() - 1, tx.blockHeight);
  ASSERT_EQ(SENT, tx.totalAmount);
  ASSERT_EQ(FEE, tx.fee);
  ASSERT_EQ(11, tx.unlockTime);
  ASSERT_EQ(extra, removeTxPublicKey(tx.extra));
}

TEST_F(WalletApi, notEnoughMoney) {
  generateAndUnlockMoney();
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, TEST_BLOCK_REWARD, FEE));
}

TEST_F(WalletApi, changePassword) {
  generateAndUnlockMoney();

  ASSERT_NO_THROW(alice.changePassword("pass", "pass2"));

  std::stringstream data;
  alice.save(data, false, false);

  CryptoNote::WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_NO_THROW(bob.load(data, "pass2"));

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, changePasswordWrong) {
  ASSERT_ANY_THROW(alice.changePassword("pass2", "pass3"));
}

TEST_F(WalletApi, shutdownInit) {
  generateBlockReward();
  node.updateObservers();
  waitPendingBalanceUpdated(0);

  alice.shutdown();
  alice.initialize("p");

  EXPECT_EQ(0, alice.getAddressCount());
  EXPECT_EQ(0, alice.getActualBalance());
  EXPECT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, detachBlockchain) {
  generateAndUnlockMoney();

  auto alicePrev = alice.getActualBalance();

  node.startAlternativeChain(1);
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  node.updateObservers();
  waitActualBalanceUpdated(alicePrev);

  ASSERT_EQ(0, alice.getActualBalance());
  ASSERT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, deleteAddresses) {
  fillWalletWithDetailsCache();
  alice.createAddress();

  for (size_t i = 0; i < 11; ++i) {
    alice.deleteAddress(alice.getAddress(0));
  }

  EXPECT_EQ(0, alice.getActualBalance());
  EXPECT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, incomingTxTransfer) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize("pass2");
  bob.createAddress();
  bob.createAddress();

  sendMoney(bob.getAddress(0), SENT, FEE);
  sendMoney(bob.getAddress(1), 2 * SENT, FEE);
  node.updateObservers();
  waitForTransactionCount(bob, 2);

  EXPECT_EQ(1, bob.getTransactionTransferCount(0));
  ASSERT_EQ(1, bob.getTransactionTransferCount(1));

  auto tr1 = bob.getTransactionTransfer(0, 0);
  EXPECT_EQ(tr1.address, bob.getAddress(0));
  EXPECT_EQ(tr1.amount, SENT);

  auto tr2 = bob.getTransactionTransfer(1, 0);
  EXPECT_EQ(tr2.address, bob.getAddress(1));
  EXPECT_EQ(tr2.amount, 2 * SENT);

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, hybridTxTransfer) {
  generateAndUnlockMoney();

  alice.createAddress();
  alice.createAddress();

  CryptoNote::WalletTransfer tr1 { alice.getAddress(1), static_cast<int64_t>(SENT) };
  CryptoNote::WalletTransfer tr2 { alice.getAddress(2), static_cast<int64_t>(2 * SENT) };

  alice.transfer({tr1, tr2}, FEE);
  node.updateObservers();
  dispatcher.yield();

  ASSERT_EQ(2, alice.getTransactionTransferCount(1));

  EXPECT_EQ(tr1.address, alice.getTransactionTransfer(1, 0).address);
  EXPECT_EQ(-tr1.amount, alice.getTransactionTransfer(1, 0).amount);

  EXPECT_EQ(tr2.address, alice.getTransactionTransfer(1, 1).address);
  EXPECT_EQ(-tr2.amount, alice.getTransactionTransfer(1, 1).amount);
}

TEST_F(WalletApi, doubleSpendJustSentOut) {
  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE);
  unlockMoney();

  sendMoney(RANDOM_ADDRESS, SENT, FEE);
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE));
}

TEST_F(WalletApi, syncAfterLoad) {
  std::stringstream data;
  alice.save(data, true, true);
  alice.shutdown();

  generateBlockReward();
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());

  alice.load(data, "pass");

  wait(300);

  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getActualBalance());
}

class INodeNoRelay : public INodeTrivialRefreshStub {
public:
  INodeNoRelay(TestBlockchainGenerator& generator) : INodeTrivialRefreshStub(generator) {}

  virtual void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) {
    m_asyncCounter.addAsyncContext();
    std::thread task(&INodeNoRelay::doNoRelayTransaction, this, transaction, callback);
    task.detach();
  }

  void doNoRelayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback)
  {
    callback(std::error_code());
    m_asyncCounter.delAsyncContext();
  }
};

TEST_F(WalletApi, DISABLED_loadTest) {
  using namespace std::chrono;

  INodeNoRelay noRelayNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, noRelayNode, TRANSACTION_SOFTLOCK_TIME);
  wallet.initialize("pass");

  const size_t ADDRESSES_COUNT = 1000;

  std::cout << "creating addresses" << std::endl;
  steady_clock::time_point start = steady_clock::now();

  for (size_t i = 0; i < ADDRESSES_COUNT; ++i) {
    wallet.createAddress();
  }

  steady_clock::time_point end = steady_clock::now();
  std::cout << "addresses creation finished in: " << duration_cast<milliseconds>(end - start).count() << " ms" << std::endl;
  std::cout << "filling up the wallets" << std::endl;

  for (size_t i = 0; i < ADDRESSES_COUNT; ++i) {
    if (!(i % 100)) {
      std::cout << "filling " << i << "th wallet" << std::endl;
    }
    generator.generateTransactionsInOneBlock(parseAddress(wallet.getAddress(i)), 10);
    generator.generateTransactionsInOneBlock(parseAddress(wallet.getAddress(i)), 10);
    generator.generateTransactionsInOneBlock(parseAddress(wallet.getAddress(i)), 10);
    generator.generateTransactionsInOneBlock(parseAddress(wallet.getAddress(i)), 10);
    generator.generateTransactionsInOneBlock(parseAddress(wallet.getAddress(i)), 10);
  }

  std::cout << "wallets filled. input any character" << std::endl;
  char x;
  std::cin >> x;

  std::cout << "sync start" << std::endl;
  steady_clock::time_point syncStart = steady_clock::now();
  noRelayNode.updateObservers();
  waitForTransactionCount(wallet, ADDRESSES_COUNT * 50);
  steady_clock::time_point syncEnd = steady_clock::now();
  std::cout << "sync took: " << duration_cast<milliseconds>(syncEnd - syncStart).count() << " ms" << std::endl;

  unlockMoney(wallet, noRelayNode);

  const size_t TRANSACTIONS_COUNT = 1000;
  std::cout << "wallets filled. input any character" << std::endl;
  std::cin >> x;

  steady_clock::time_point transferStart = steady_clock::now();
  for (size_t i = 0; i < TRANSACTIONS_COUNT; ++i) {
    CryptoNote::WalletTransfer tr;
    tr.amount = SENT;
    tr.address = RANDOM_ADDRESS;
    wallet.transfer(tr, FEE);
  }
  steady_clock::time_point transferEnd = steady_clock::now();
  std::cout << "transfers took: " << duration_cast<milliseconds>(transferEnd - transferStart).count() << " ms" << std::endl;

  wallet.shutdown();
  wait(100);
}

TEST_F(WalletApi, transferSmallFeeTransactionThrows) {
  generateAndUnlockMoney();

  ASSERT_ANY_THROW(sendMoneyToRandomAddressFrom(alice.getAddress(0), SENT, currency.minimumFee() - 1));
}

TEST_F(WalletApi, initializeWithKeysSucceded) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);

  CryptoNote::KeyPair viewKeys;
  Crypto::generate_keys(viewKeys.publicKey, viewKeys.secretKey);
  ASSERT_NO_THROW(wallet.initializeWithViewKey(viewKeys.secretKey, "pass"));

  wallet.shutdown();
}

TEST_F(WalletApi, initializeWithKeysThrowsIfAlreadInitialized) {
  CryptoNote::KeyPair viewKeys;
  Crypto::generate_keys(viewKeys.publicKey, viewKeys.secretKey);

  ASSERT_ANY_THROW(alice.initializeWithViewKey(viewKeys.secretKey, "pass"));
}

TEST_F(WalletApi, initializeWithKeysThrowsIfStopped) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  wallet.stop();

  CryptoNote::KeyPair viewKeys;
  Crypto::generate_keys(viewKeys.publicKey, viewKeys.secretKey);
  ASSERT_ANY_THROW(wallet.initializeWithViewKey(viewKeys.secretKey, "pass"));
}

TEST_F(WalletApi, getViewKeyReturnsProperKey) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);

  CryptoNote::KeyPair viewKeys;
  Crypto::generate_keys(viewKeys.publicKey, viewKeys.secretKey);
  wallet.initializeWithViewKey(viewKeys.secretKey, "pass");

  CryptoNote::KeyPair retrievedKeys = wallet.getViewKey();
  ASSERT_EQ(viewKeys.publicKey, retrievedKeys.publicKey);
  ASSERT_EQ(viewKeys.secretKey, retrievedKeys.secretKey);

  wallet.shutdown();
}

TEST_F(WalletApi, getViewKeyThrowsIfNotInitialized) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  ASSERT_ANY_THROW(wallet.getViewKey());
}

TEST_F(WalletApi, getViewKeyThrowsIfStopped) {
  alice.stop();

  ASSERT_ANY_THROW(alice.getViewKey());
}

TEST_F(WalletApi, getAddressSpendKeyReturnsProperKey) {
  CryptoNote::KeyPair spendKeys;
  Crypto::generate_keys(spendKeys.publicKey, spendKeys.secretKey);

  alice.createAddress(spendKeys.secretKey);

  CryptoNote::KeyPair retrievedKeys = alice.getAddressSpendKey(1);
  ASSERT_EQ(spendKeys.publicKey, retrievedKeys.publicKey);
  ASSERT_EQ(spendKeys.secretKey, retrievedKeys.secretKey);
}

TEST_F(WalletApi, getAddressSpendKeyThrowsForWrongAddressIndex) {
  ASSERT_ANY_THROW(alice.getAddressSpendKey(1));
}

TEST_F(WalletApi, getAddressSpendKeyThrowsIfNotInitialized) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  ASSERT_ANY_THROW(wallet.getAddressSpendKey(0));
}

TEST_F(WalletApi, getAddressSpendKeyThrowsIfStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.getAddressSpendKey(0));
}

Crypto::PublicKey generatePublicKey() {
  CryptoNote::KeyPair spendKeys;
  Crypto::generate_keys(spendKeys.publicKey, spendKeys.secretKey);

  return spendKeys.publicKey;
}

TEST_F(WalletApi, createTrackingKeyAddressSucceeded) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  wallet.initialize("pass");

  Crypto::PublicKey publicKey = generatePublicKey();

  ASSERT_NO_THROW(wallet.createAddress(publicKey));
  ASSERT_EQ(1, wallet.getAddressCount());
  wallet.shutdown();
}

TEST_F(WalletApi, createTrackingKeyThrowsIfNotInitialized) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);

  Crypto::PublicKey publicKey = generatePublicKey();
  ASSERT_ANY_THROW(wallet.createAddress(publicKey));
}

TEST_F(WalletApi, createTrackingKeyThrowsIfStopped) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  wallet.initialize("pass");
  wallet.stop();

  Crypto::PublicKey publicKey = generatePublicKey();
  ASSERT_ANY_THROW(wallet.createAddress(publicKey));
  wallet.shutdown();
}

TEST_F(WalletApi, createTrackingKeyThrowsIfKeyExists) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  wallet.initialize("pass");

  Crypto::PublicKey publicKey = generatePublicKey();
  wallet.createAddress(publicKey);
  ASSERT_ANY_THROW(wallet.createAddress(publicKey));
  wallet.shutdown();
}

TEST_F(WalletApi, createTrackingKeyThrowsIfWalletHasNotTrackingKeys) {
  Crypto::PublicKey publicKey = generatePublicKey();
  ASSERT_ANY_THROW(alice.createAddress(publicKey));
}

TEST_F(WalletApi, getAddressSpendKeyForTrackingKeyReturnsNullSecretKey) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  wallet.initialize("pass");

  Crypto::PublicKey publicKey = generatePublicKey();
  wallet.createAddress(publicKey);

  KeyPair spendKeys = wallet.getAddressSpendKey(0);
  ASSERT_EQ(NULL_SECRET_KEY, spendKeys.secretKey);

  wallet.shutdown();
}

TEST_F(WalletApi, trackingAddressReceivesMoney) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node);
  bob.initialize("pass2");

  Crypto::PublicKey publicKey = generatePublicKey();
  bob.createAddress(publicKey);

  sendMoney(bob.getAddress(0), SENT, FEE);
  node.updateObservers();

  auto expectedTransactionHeight = node.getLastKnownBlockHeight();
  waitPendingBalanceUpdated(bob, 0);

  ASSERT_EQ(SENT, bob.getPendingBalance());
  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(1, bob.getTransactionCount());

  CryptoNote::WalletTransaction transaction = bob.getTransaction(0);
  ASSERT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, transaction.state);
  ASSERT_EQ(expectedTransactionHeight, transaction.blockHeight);
  ASSERT_EQ(SENT, transaction.totalAmount);
  ASSERT_EQ(FEE, transaction.fee);
  ASSERT_EQ(0, transaction.unlockTime);

  bob.shutdown();
}

TEST_F(WalletApi, trackingAddressUnlocksMoney) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node);
  bob.initialize("pass2");

  Crypto::PublicKey publicKey = generatePublicKey();
  bob.createAddress(publicKey);

  sendMoney(bob.getAddress(0), SENT, FEE);
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  node.updateObservers();
  waitActualBalanceUpdated(bob, 0);

  ASSERT_EQ(0, bob.getPendingBalance());
  ASSERT_EQ(SENT, bob.getActualBalance());
}

TEST_F(WalletApi, transferFromTrackingKeyThrows) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node);
  bob.initialize("pass2");

  Crypto::PublicKey publicKey = generatePublicKey();
  bob.createAddress(publicKey);

  sendMoney(bob.getAddress(0), SENT, FEE);
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  node.updateObservers();
  waitActualBalanceUpdated(bob, 0);

  ASSERT_ANY_THROW(sendMoney(bob, RANDOM_ADDRESS, SENT, FEE));
  bob.shutdown();
}

TEST_F(WalletApi, walletGetsSyncCompletedEvent) {
  generator.generateEmptyBlocks(1);
  node.updateObservers();

  ASSERT_TRUE(waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5)));
}

TEST_F(WalletApi, walletGetsSyncProgressUpdatedEvent) {
  generator.generateEmptyBlocks(1);
  node.updateObservers();

  ASSERT_TRUE(waitForWalletEvent(alice, CryptoNote::SYNC_PROGRESS_UPDATED, std::chrono::seconds(5)));
}

struct CatchTransactionNodeStub : public INodeTrivialRefreshStub {
  CatchTransactionNodeStub(TestBlockchainGenerator& generator): INodeTrivialRefreshStub(generator), caught(false) {}

  virtual void relayTransaction(const CryptoNote::Transaction& incomingTransaction, const Callback& callback) override {
    transaction = incomingTransaction;
    caught = true;
    INodeTrivialRefreshStub::relayTransaction(incomingTransaction, callback);
  }

  bool caught;
  CryptoNote::Transaction transaction;
};

TEST_F(WalletApi, createFusionTransactionCreatesValidFusionTransactionWithoutMixin) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode);
  wallet.initialize("pass");
  wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, node, currency, FUSION_THRESHOLD);

  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, wallet.createFusionTransaction(FUSION_THRESHOLD, 0));
  ASSERT_TRUE(catchNode.caught);
  ASSERT_TRUE(currency.isFusionTransaction(catchNode.transaction));

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionCreatesValidFusionTransactionWithMixin) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode);
  wallet.initialize("pass");
  wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, node, currency, FUSION_THRESHOLD);

  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, wallet.createFusionTransaction(FUSION_THRESHOLD, 2));
  ASSERT_TRUE(catchNode.caught);
  ASSERT_TRUE(currency.isFusionTransaction(catchNode.transaction));

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionDoesnotAffectTotalBalance) {
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD);

  auto totalBalance = alice.getActualBalance() + alice.getPendingBalance();
  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, alice.createFusionTransaction(FUSION_THRESHOLD, 2));
  ASSERT_EQ(totalBalance, alice.getActualBalance() + alice.getPendingBalance());
}

TEST_F(WalletApi, createFusionTransactionFailsIfMixinToobig) {
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD);
  ASSERT_ANY_THROW(alice.createFusionTransaction(FUSION_THRESHOLD, 10000000));
}

TEST_F(WalletApi, createFusionTransactionFailsIfNoTransfers) {
  ASSERT_EQ(WALLET_INVALID_TRANSACTION_ID, alice.createFusionTransaction(FUSION_THRESHOLD, 0));
}

TEST_F(WalletApi, createFusionTransactionThrowsIfNotInitialized) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  ASSERT_ANY_THROW(wallet.createFusionTransaction(FUSION_THRESHOLD, 0));
}

TEST_F(WalletApi, createFusionTransactionThrowsIfStopped) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  wallet.initialize("pass");
  wallet.stop();
  ASSERT_ANY_THROW(wallet.createFusionTransaction(FUSION_THRESHOLD, 0));
  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionThrowsIfThresholdTooSmall) {
  ASSERT_ANY_THROW(alice.createFusionTransaction(currency.defaultDustThreshold() - 1, 0));
}

TEST_F(WalletApi, createFusionTransactionThrowsIfNoAddresses) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  wallet.initialize("pass");
  ASSERT_ANY_THROW(wallet.createFusionTransaction(FUSION_THRESHOLD, 0));
  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionThrowsIfTransactionSendError) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode);
  wallet.initialize("pass");
  wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, node, currency, FUSION_THRESHOLD);

  catchNode.setNextTransactionError();
  ASSERT_ANY_THROW(wallet.createFusionTransaction(FUSION_THRESHOLD, 0));
  wallet.shutdown();
}

TEST_F(WalletApi, fusionManagerEstimateThrowsIfNotInitialized) {
  const uint64_t THRESHOLD = 100;
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  ASSERT_ANY_THROW(wallet.estimate(THRESHOLD));
}

TEST_F(WalletApi, fusionManagerEstimateThrowsIfStopped) {
  const uint64_t THRESHOLD = 100;
  alice.stop();
  ASSERT_ANY_THROW(alice.estimate(THRESHOLD));
}

TEST_F(WalletApi, fusionManagerEstimateEmpty) {
  const uint64_t THRESHOLD = 100;
  IFusionManager::EstimateResult emptyResult = {0, 0};
  ASSERT_EQ(emptyResult, alice.estimate(THRESHOLD));
}

TEST_F(WalletApi, fusionManagerEstimateLocked) {
  auto pending = alice.getPendingBalance();
  generateBlockReward();
  node.updateObservers();
  waitPendingBalanceUpdated(alice, pending);

  IFusionManager::EstimateResult expectedResult =  {0, 0};
  ASSERT_EQ(expectedResult, alice.estimate(0));
}

TEST_F(WalletApi, fusionManagerEstimateNullThreshold) {
  generateAndUnlockMoney();

  ASSERT_EQ(1, alice.getTransactionCount());
  CryptoNote::Transaction tx = boost::value_initialized<CryptoNote::Transaction>();
  ASSERT_TRUE(generator.getTransactionByHash(alice.getTransaction(0).hash, tx, false));
  ASSERT_FALSE(tx.outputs.empty());

  IFusionManager::EstimateResult expectedResult = {0, tx.outputs.size()};
  ASSERT_EQ(expectedResult, alice.estimate(0));
}

TEST_F(WalletApi, DISABLED_fusionManagerEstimate) {
  generateAndUnlockMoney();

  ASSERT_EQ(1, alice.getTransactionCount());
  CryptoNote::Transaction tx = boost::value_initialized<CryptoNote::Transaction>();
  ASSERT_TRUE(generator.getTransactionByHash(alice.getTransaction(0).hash, tx, false));
  ASSERT_FALSE(tx.outputs.empty());

  IFusionManager::EstimateResult expectedResult = {0, tx.outputs.size()};
  size_t maxOutputIndex = 0;
  uint64_t maxOutputAmount = 0;
  for (size_t i = 0; i < tx.outputs.size(); ++i) {
    if (tx.outputs[i].amount > maxOutputAmount) {
      maxOutputAmount = tx.outputs[i].amount;
      maxOutputIndex = i;
    }

    if (currency.isAmountApplicableInFusionTransactionInput(tx.outputs[i].amount, tx.outputs[i].amount + 1)) {
      ++expectedResult.fusionReadyCount;
    }
  }

  ASSERT_EQ(expectedResult, alice.estimate(tx.outputs[maxOutputIndex].amount + 1));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionThrowsIfNotInitialized) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  ASSERT_ANY_THROW(wallet.isFusionTransaction(0));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionThrowsIfStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.isFusionTransaction(0));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionEmpty) {
  ASSERT_ANY_THROW(alice.isFusionTransaction(0));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionNotFusion) {
  generateAndUnlockMoney();

  ASSERT_EQ(1, alice.getTransactionCount());
  ASSERT_FALSE(alice.isFusionTransaction(0));
}

TEST_F(WalletApi, fusionManagerIsFusionTransaction) {
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD);

  auto id = alice.createFusionTransaction(FUSION_THRESHOLD, 0);
  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, id);

  node.updateObservers();
  waitForTransactionUpdated(alice, id);

  ASSERT_TRUE(alice.isFusionTransaction(id));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionNotInTransfersContainer) {
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD);

  auto id = alice.createFusionTransaction(FUSION_THRESHOLD, 0);
  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, id);

  ASSERT_TRUE(alice.isFusionTransaction(id));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionThrowsIfOutOfRange) {
  ASSERT_ANY_THROW(alice.isFusionTransaction(1));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionSpent) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node);
  wallet.initialize("pass");
  wallet.createAddress();

  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD);

  auto id = alice.createFusionTransaction(FUSION_THRESHOLD, 0);
  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, id);

  unlockMoney();
  CryptoNote::WalletTransfer transfer;
  transfer.address = wallet.getAddress(0);
  transfer.amount = alice.getActualBalance() - currency.minimumFee();
  alice.transfer(aliceAddress, transfer, currency.minimumFee(), 0);

  auto pending = wallet.getPendingBalance();
  node.updateObservers();
  waitPendingBalanceUpdated(wallet, pending);

  ASSERT_TRUE(alice.isFusionTransaction(id));
}

