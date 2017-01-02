// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include <future>
#include <chrono>
#include <array>

#include "EventWaiter.h"
#include "INode.h"
#include "WalletLegacy/WalletLegacy.h"
#include "WalletLegacy/WalletHelper.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNote.h"

#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"
#include <Logging/ConsoleLogger.h>

class TrivialWalletObserver : public CryptoNote::IWalletLegacyObserver
{
public:
  TrivialWalletObserver() : actualBalance(0), pendingBalance(0) {}

  bool waitForSyncEnd() {
    return synced.wait_for(std::chrono::milliseconds(3000));
  }

  bool waitForSendEnd(std::error_code& ec) {
    if (!sent.wait_for(std::chrono::milliseconds(5000))) return false;
    ec = sendResult;
    return true;
  }

  bool waitForSaveEnd(std::error_code& ec) {
    if (!saved.wait_for(std::chrono::milliseconds(5000))) return false;
    ec = saveResult;
    return true;
  }

  bool waitForLoadEnd(std::error_code& ec) {
    if (!loaden.wait_for(std::chrono::milliseconds(5000))) return false;
    ec = loadResult;
    return true;
  }

  bool waitForDepositsUpdated() {
    if (!depositsUpdate.wait_for(std::chrono::milliseconds(5000))) return false;
    return true;
  }

  virtual void synchronizationCompleted(std::error_code result) override {
    synced.notify();
  }

  virtual void sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) override {
    sendResult = result;
    sent.notify();
  }

  virtual void saveCompleted(std::error_code result) override {
    saveResult = result;
    saved.notify();
  }

  virtual void initCompleted(std::error_code result) override {
    loadResult = result;
    loaden.notify();
  }

  virtual void actualBalanceUpdated(uint64_t actualBalance) override {
    //    std::cout << "actual balance: " << actualBalance << std::endl;
    this->actualBalance = actualBalance;
  }

  virtual void pendingBalanceUpdated(uint64_t pendingBalance) override {
//    std::cout << "pending balance: " << pendingBalance << std::endl;
    this->pendingBalance = pendingBalance;
  }

  virtual void depositsUpdated(const std::vector<CryptoNote::DepositId>& depositIds) override {
    depositsUpdate.notify();
  }

  std::error_code sendResult;
  std::error_code saveResult;
  std::error_code loadResult;

  std::atomic<uint64_t> actualBalance;
  std::atomic<uint64_t> pendingBalance;

  EventWaiter synced;
  EventWaiter saved;
  EventWaiter loaden;
  EventWaiter sent;
  EventWaiter depositsUpdate;
};

struct SaveOnInitWalletObserver: public CryptoNote::IWalletLegacyObserver {
  SaveOnInitWalletObserver(CryptoNote::WalletLegacy* wallet) : wallet(wallet) {
  };
  virtual ~SaveOnInitWalletObserver() {}

  virtual void initCompleted(std::error_code result) override {
    wallet->save(stream, true, true);
  }

  CryptoNote::WalletLegacy* wallet;
  std::stringstream stream;
};

static const uint64_t TEST_BLOCK_REWARD = CryptoNote::START_BLOCK_REWARD;

CryptoNote::TransactionId TransferMoney(CryptoNote::WalletLegacy& from, CryptoNote::WalletLegacy& to, int64_t amount, uint64_t fee,
    uint64_t mixIn = 0, const std::string& extra = "", const std::vector<CryptoNote::TransactionMessage>& messages = std::vector<CryptoNote::TransactionMessage>()) {
  CryptoNote::WalletLegacyTransfer transfer;
  transfer.amount = amount;
  transfer.address = to.getAddress();

  return from.sendTransaction(transfer, fee, extra, mixIn, 0, messages);
}

void WaitWalletSync(TrivialWalletObserver* observer) {
  ASSERT_TRUE(observer->waitForSyncEnd());
}

void WaitWalletSend(TrivialWalletObserver* observer) {
  std::error_code ec;
  ASSERT_TRUE(observer->waitForSendEnd(ec));
  ASSERT_FALSE(ec);
}

void WaitWalletSend(TrivialWalletObserver* observer, std::error_code& ec) {
  ASSERT_TRUE(observer->waitForSendEnd(ec));
}

void WaitWalletSave(TrivialWalletObserver* observer) {
  std::error_code ec;

  ASSERT_TRUE(observer->waitForSaveEnd(ec));
  EXPECT_FALSE(ec);
}

void WaitWalletLoad(TrivialWalletObserver* observer) {
  std::error_code ec;
  
  ASSERT_TRUE(observer->waitForLoadEnd(ec));
  EXPECT_FALSE(ec);  
}

class ScopedObserverBase : public CryptoNote::IWalletLegacyObserver {
public:
  ScopedObserverBase(CryptoNote::IWalletLegacy& wallet) : m_wallet(wallet) {
    m_wallet.addObserver(this);
  }

  ScopedObserverBase(const ScopedObserverBase&) = delete;
  ScopedObserverBase(ScopedObserverBase&&) = delete;

  virtual ~ScopedObserverBase() {
    m_wallet.removeObserver(this);
  }

protected:
  CryptoNote::IWalletLegacy& m_wallet;
  EventWaiter called;
};

class DepositsUpdatedScopedObserver : public ScopedObserverBase {
public:
  DepositsUpdatedScopedObserver(CryptoNote::IWalletLegacy& wallet) : ScopedObserverBase(wallet) {}
  virtual ~DepositsUpdatedScopedObserver() {}

  virtual void depositsUpdated(const std::vector<CryptoNote::DepositId>& depositIds) override {
    m_updatedDeposits = depositIds;
    called.notify();
  }

  std::vector<CryptoNote::DepositId> wait() {
    if (!called.wait_for(std::chrono::milliseconds(5000))) {
      throw std::runtime_error("Operation timeout");
    }

    return m_updatedDeposits;
  }

private:
  std::vector<CryptoNote::DepositId> m_updatedDeposits;
};

class DepositsActualBalanceChangedScopedObserver : public ScopedObserverBase {
public:
  DepositsActualBalanceChangedScopedObserver(CryptoNote::IWalletLegacy& wallet) : ScopedObserverBase(wallet) {}
  virtual ~DepositsActualBalanceChangedScopedObserver() {}

  virtual void actualDepositBalanceUpdated(uint64_t actualDepositBalance) override {
    m_actualBalance = actualDepositBalance;
    called.notify();
  }

  uint64_t wait() {
    if (!called.wait_for(std::chrono::milliseconds(5000))) {
      throw std::runtime_error("Operation timeout");
    }

    return m_actualBalance;
  }

private:
  uint64_t m_actualBalance;
};

class DepositsPendingBalanceChangedScopedObserver : public ScopedObserverBase {
public:
  DepositsPendingBalanceChangedScopedObserver(CryptoNote::IWalletLegacy& wallet) : ScopedObserverBase(wallet) {}
  virtual ~DepositsPendingBalanceChangedScopedObserver() {}

  virtual void pendingDepositBalanceUpdated(uint64_t pendingDepositBalance) override {
    m_pendingBalance = pendingDepositBalance;
    called.notify();
  }

  uint64_t wait() {
    if (!called.wait_for(std::chrono::milliseconds(5000))) {
      throw std::runtime_error("Operation timeout");
    }

    return m_pendingBalance;
  }

private:
  uint64_t m_pendingBalance;
};

class PendingBalanceChangedScopedObserver : public ScopedObserverBase {
public:
  PendingBalanceChangedScopedObserver(CryptoNote::IWalletLegacy& wallet) : ScopedObserverBase(wallet) {}
  virtual ~PendingBalanceChangedScopedObserver() {}

  virtual void pendingBalanceUpdated(uint64_t pendingBalance) override {
    m_pendingBalance = pendingBalance;
    called.notify();
  }

  uint64_t wait() {
    if (!called.wait_for(std::chrono::milliseconds(5000))) {
      throw std::runtime_error("Operation timeout");
    }

    return m_pendingBalance;
  }

private:
  uint64_t m_pendingBalance;
};

class ActualBalanceChangedScopedObserver : public ScopedObserverBase {
public:
  ActualBalanceChangedScopedObserver(CryptoNote::IWalletLegacy& wallet) : ScopedObserverBase(wallet) {}
  virtual ~ActualBalanceChangedScopedObserver() {}

  virtual void actualBalanceUpdated(uint64_t actualBalance) override {
    m_actualBalance = actualBalance;
    called.notify();
  }

  uint64_t wait() {
    if (!called.wait_for(std::chrono::milliseconds(5000))) {
      throw std::runtime_error("Operation timeout");
    }

    return m_actualBalance;
  }

private:
  uint64_t m_actualBalance;
};

class WalletLegacyApi : public ::testing::Test
{
public:
  WalletLegacyApi() : m_currency(CryptoNote::CurrencyBuilder(m_logger).depositMinTerm(100).depositMinTotalRateFactor(0).defaultDustThreshold(0).currency()), generator(m_currency) {
  }

  void SetUp() override;

protected:
  void prepareAliceWallet();
  void prepareBobWallet();
  void prepareCarolWallet();

  void GetOneBlockReward(CryptoNote::WalletLegacy& wallet);
  void GetOneBlockReward(CryptoNote::WalletLegacy& wallet, const CryptoNote::Currency& currency, TestBlockchainGenerator& blockchainGenerator);
  void GetOneBlockRewardAndUnlock(CryptoNote::WalletLegacy& wallet, TrivialWalletObserver& observer, INodeTrivialRefreshStub& node,
                                  const CryptoNote::Currency& currency, TestBlockchainGenerator& blockchainGenerator);
  void GenerateOneBlockRewardAndUnlock();

  void TestSendMoney(int64_t transferAmount, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "");
  void performTransferWithErrorTx(const std::array<int64_t, 5>& amounts, uint64_t fee);

  CryptoNote::DepositId makeDeposit(uint64_t amount, uint32_t term, uint64_t fee, uint64_t mixin = 0);
  void unlockDeposit(uint32_t term);
  CryptoNote::DepositId makeDepositAndUnlock(uint64_t amount, uint32_t term, uint64_t fee, uint64_t mixin = 0);
  CryptoNote::TransactionId withdrawDeposits(const std::vector<CryptoNote::DepositId>& ids, uint64_t fee);
  uint64_t calculateTotalDepositAmount(uint64_t amount, uint32_t term);

  Logging::ConsoleLogger m_logger;
  CryptoNote::Currency m_currency;

  TestBlockchainGenerator generator;

  std::shared_ptr<TrivialWalletObserver> aliceWalletObserver;
  std::shared_ptr<INodeTrivialRefreshStub> aliceNode;
  std::shared_ptr<CryptoNote::WalletLegacy> alice;

  std::shared_ptr<TrivialWalletObserver> bobWalletObserver;
  std::shared_ptr<INodeTrivialRefreshStub> bobNode;
  std::shared_ptr<CryptoNote::WalletLegacy> bob;

  std::shared_ptr<TrivialWalletObserver> carolWalletObserver;
  std::shared_ptr<INodeTrivialRefreshStub> carolNode;
  std::shared_ptr<CryptoNote::WalletLegacy> carol;
};

void WalletLegacyApi::SetUp() {
  prepareAliceWallet();
  generator.generateEmptyBlocks(3);
}

void WalletLegacyApi::prepareAliceWallet() {
  decltype(aliceNode) newNode(new INodeTrivialRefreshStub(generator));

  alice.reset(new CryptoNote::WalletLegacy(m_currency, *newNode));
  aliceNode = newNode;

  aliceWalletObserver.reset(new TrivialWalletObserver());
  alice->addObserver(aliceWalletObserver.get());
}

void WalletLegacyApi::prepareBobWallet() {
  bobNode.reset(new INodeTrivialRefreshStub(generator));
  bobWalletObserver.reset(new TrivialWalletObserver());

  bob.reset(new CryptoNote::WalletLegacy(m_currency, *bobNode));
  bob->addObserver(bobWalletObserver.get());
}

void WalletLegacyApi::prepareCarolWallet() {
  carolNode.reset(new INodeTrivialRefreshStub(generator));
  carolWalletObserver.reset(new TrivialWalletObserver());

  carol.reset(new CryptoNote::WalletLegacy(m_currency, *carolNode));
  carol->addObserver(carolWalletObserver.get());
}

void WalletLegacyApi::GetOneBlockReward(CryptoNote::WalletLegacy& wallet) {
  GetOneBlockReward(wallet, m_currency, generator);
}

void WalletLegacyApi::GetOneBlockReward(CryptoNote::WalletLegacy& wallet, const CryptoNote::Currency& currency, TestBlockchainGenerator& blockchainGenerator) {
  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(currency.parseAccountAddressString(wallet.getAddress(), address));
  blockchainGenerator.getBlockRewardForAddress(address);
}

void WalletLegacyApi::GetOneBlockRewardAndUnlock(CryptoNote::WalletLegacy& wallet, TrivialWalletObserver& observer, INodeTrivialRefreshStub& node,
                                                 const CryptoNote::Currency& currency, TestBlockchainGenerator& blockchainGenerator) {
  GetOneBlockReward(wallet, currency, blockchainGenerator);
  blockchainGenerator.generateEmptyBlocks(10);
  node.updateObservers();
  WaitWalletSync(&observer);
}

//may be called only after prepareAliceWallet and alice->initAndGenerate
void WalletLegacyApi::GenerateOneBlockRewardAndUnlock() {
  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
}

void WalletLegacyApi::performTransferWithErrorTx(const std::array<int64_t, 5>& amounts, uint64_t fee) {
  std::vector<CryptoNote::WalletLegacyTransfer> trs;
  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = amounts[0];
  trs.push_back(tr);

  tr.address = bob->getAddress();
  tr.amount = amounts[1];
  trs.push_back(tr);

  tr.address = carol->getAddress();
  tr.amount = amounts[2];
  trs.push_back(tr);

  aliceNode->setNextTransactionError();
  alice->sendTransaction(trs, fee);

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), result));
  ASSERT_NE(result.value(), 0);

  trs.clear();

  tr.address = bob->getAddress();
  tr.amount = amounts[3];
  trs.push_back(tr);

  tr.address = carol->getAddress();
  tr.amount = amounts[4];
  trs.push_back(tr);

  alice->sendTransaction(trs, fee);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);
}

void WalletLegacyApi::TestSendMoney(int64_t transferAmount, uint64_t fee, uint64_t mixIn, const std::string& extra) {
  prepareBobWallet();

  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  //unlock Alice's money
  generator.generateEmptyBlocks(10);
  uint64_t expectedBalance = TEST_BLOCK_REWARD;

  aliceNode->updateObservers();

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(0, alice->pendingBalance());
  EXPECT_EQ(expectedBalance, alice->actualBalance());

  EXPECT_EQ(expectedBalance, aliceWalletObserver->actualBalance);
  EXPECT_EQ(0, aliceWalletObserver->pendingBalance);

  bob->initAndGenerate("pass2");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, transferAmount, fee, mixIn, ""));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  EXPECT_EQ(0, bob->pendingBalance());
  EXPECT_EQ(transferAmount, bob->actualBalance());

  EXPECT_EQ(0, alice->pendingBalance());
  EXPECT_EQ(expectedBalance - transferAmount - fee, alice->actualBalance());

  alice->shutdown();
  bob->shutdown();
}

void WaitWalletLoad(TrivialWalletObserver* observer, std::error_code& ec) {
  ASSERT_TRUE(observer->waitForLoadEnd(ec));
}

CryptoNote::DepositId WalletLegacyApi::makeDeposit(uint64_t amount, uint32_t term, uint64_t fee, uint64_t mixin) {
  auto txId = alice->deposit(term, amount, fee, mixin);
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());

  CryptoNote::WalletLegacyTransaction txInfo;
  alice->getTransaction(txId, txInfo);

  return txInfo.firstDepositId;
}

void WalletLegacyApi::unlockDeposit(uint32_t term) {
  generator.generateEmptyBlocks(term - 1); //subtract 1 becaause INodeTrivialRefreshStub->relayTransaction adds new block implicitly
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());
}

CryptoNote::DepositId WalletLegacyApi::makeDepositAndUnlock(uint64_t amount, uint32_t term, uint64_t fee, uint64_t mixin) {
  auto id = makeDeposit(amount, term, fee, mixin);
  unlockDeposit(term);

  return id;
}

CryptoNote::TransactionId WalletLegacyApi::withdrawDeposits(const std::vector<CryptoNote::DepositId>& ids, uint64_t fee) {
  auto txId = alice->withdrawDeposits(ids, fee);
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());

  return txId;
}

uint64_t WalletLegacyApi::calculateTotalDepositAmount(uint64_t amount, uint32_t term) {
  return m_currency.calculateInterest(amount, term) + amount;
}

TEST_F(WalletLegacyApi, initAndSave) {
  SaveOnInitWalletObserver saveOnInit(alice.get());
  alice->addObserver(&saveOnInit);
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));
  alice->shutdown();
}

TEST_F(WalletLegacyApi, refreshWithMoney) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(alice->actualBalance(), 0);
  ASSERT_EQ(alice->pendingBalance(), 0);

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getBlockRewardForAddress(address);

  aliceNode->updateObservers();

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->actualBalance(), 0);
  EXPECT_EQ(alice->pendingBalance(), TEST_BLOCK_REWARD);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, initWithMoney) {
  std::stringstream archive;

  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  alice->save(archive, true, true);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  ASSERT_EQ(alice->actualBalance(), 0);
  ASSERT_EQ(alice->pendingBalance(), 0);

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));

  alice->shutdown();

  generator.getBlockRewardForAddress(address);

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->actualBalance(), 0);
  EXPECT_EQ(alice->pendingBalance(), TEST_BLOCK_REWARD);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, TransactionsAndTransfersAfterSend) {
  prepareBobWallet();
  prepareCarolWallet();

  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->getTransactionCount(), 0);
  EXPECT_EQ(alice->getTransferCount(), 0);

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  //unblock Alice's money
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->getTransactionCount(), 1);

  bob->initAndGenerate("pass2");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t fee = 100000;
  int64_t amount1 = 1230000;
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, amount1, fee, 0));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  int64_t amount2 = 1234500;
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, amount2, fee, 0));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  int64_t amount3 = 1234567;
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, amount3, fee, 0));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  carol->initAndGenerate("pass3");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  int64_t amount4 = 1020304;
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *carol, amount4, fee, 0));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  EXPECT_EQ(alice->getTransactionCount(), 5);

  CryptoNote::WalletLegacyTransaction tx;

  //Transaction with id = 0 is tested in getTransactionSuccess
  ASSERT_TRUE(alice->getTransaction(1, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount1 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 0);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransaction(2, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount2 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 1);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransaction(3, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount3 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 2);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransaction(4, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount4 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 3);
  EXPECT_EQ(tx.transferCount, 1);

  //Now checking transfers
  CryptoNote::WalletLegacyTransfer tr;
  ASSERT_TRUE(alice->getTransfer(0, tr));
  EXPECT_EQ(tr.amount, amount1);
  EXPECT_EQ(tr.address, bob->getAddress());

  ASSERT_TRUE(alice->getTransfer(1, tr));
  EXPECT_EQ(tr.amount, amount2);
  EXPECT_EQ(tr.address, bob->getAddress());

  ASSERT_TRUE(alice->getTransfer(2, tr));
  EXPECT_EQ(tr.amount, amount3);
  EXPECT_EQ(tr.address, bob->getAddress());

  ASSERT_TRUE(alice->getTransfer(3, tr));
  EXPECT_EQ(tr.amount, amount4);
  EXPECT_EQ(tr.address, carol->getAddress());

  EXPECT_EQ(alice->findTransactionByTransferId(0), 1);
  EXPECT_EQ(alice->findTransactionByTransferId(1), 2);
  EXPECT_EQ(alice->findTransactionByTransferId(2), 3);
  EXPECT_EQ(alice->findTransactionByTransferId(3), 4);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, saveAndLoadCacheDetails) {
  prepareBobWallet();
  prepareCarolWallet();

  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  //unblock Alice's money
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  bob->initAndGenerate("pass2");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  carol->initAndGenerate("pass3");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  uint64_t fee = 1000000;
  int64_t amount1 = 1234567;
  int64_t amount2 = 1020304;
  int64_t amount3 = 2030405;

  std::vector<CryptoNote::WalletLegacyTransfer> trs;
  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = amount1;
  trs.push_back(tr);

  tr.address = bob->getAddress();
  tr.amount = amount2;
  trs.push_back(tr);

  alice->sendTransaction(trs, fee, "", 0, 0);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  trs.clear();
  tr.address = carol->getAddress();
  tr.amount = amount3;
  trs.push_back(tr);

  alice->sendTransaction(trs, fee, "", 0, 0);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  std::stringstream archive;
  alice->save(archive, true, true);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  auto prevActualBalance = alice->actualBalance();
  auto prevPendingBalance = alice->pendingBalance();

  alice->shutdown();

  prepareAliceWallet();

  alice->initAndLoad(archive, "pass");
  std::error_code ec;

  WaitWalletLoad(aliceWalletObserver.get(), ec);
  ASSERT_FALSE(ec);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(alice->getTransactionCount(), 3);
  ASSERT_EQ(alice->getTransferCount(), 3);

  EXPECT_EQ(prevActualBalance, alice->actualBalance());
  EXPECT_EQ(prevPendingBalance, alice->pendingBalance());

  CryptoNote::WalletLegacyTransaction tx;
  ASSERT_TRUE(alice->getTransaction(1, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount1 + amount2 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.firstTransferId, 0);
  EXPECT_EQ(tx.transferCount, 2);

  ASSERT_TRUE(alice->getTransaction(2, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount3 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.firstTransferId, 2);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransfer(0, tr));
  EXPECT_EQ(tr.address, bob->getAddress());
  EXPECT_EQ(tr.amount, amount1);

  ASSERT_TRUE(alice->getTransfer(1, tr));
  EXPECT_EQ(tr.address, bob->getAddress());
  EXPECT_EQ(tr.amount, amount2);

  ASSERT_TRUE(alice->getTransfer(2, tr));
  EXPECT_EQ(tr.address, carol->getAddress());
  EXPECT_EQ(tr.amount, amount3);

  EXPECT_EQ(alice->findTransactionByTransferId(0), 1);
  EXPECT_EQ(alice->findTransactionByTransferId(1), 1);
  EXPECT_EQ(alice->findTransactionByTransferId(2), 2);

  alice->shutdown();
  carol->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, sendMoneySuccessNoMixin) {
  ASSERT_NO_FATAL_FAILURE(TestSendMoney(10000000, 1000000, 0));
}

TEST_F(WalletLegacyApi, sendMoneySuccessWithMixin) {
  ASSERT_NO_FATAL_FAILURE(TestSendMoney(10000000, 1000000, 3));
}

TEST_F(WalletLegacyApi, getTransactionSuccess) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::WalletLegacyTransaction tx;

  ASSERT_EQ(alice->getTransactionCount(), 1);
  ASSERT_TRUE(alice->getTransaction(0, tx));

  EXPECT_EQ(tx.firstTransferId, CryptoNote::WALLET_LEGACY_INVALID_TRANSFER_ID);
  EXPECT_EQ(tx.transferCount, 0);
  EXPECT_EQ(tx.totalAmount, TEST_BLOCK_REWARD);
  EXPECT_EQ(tx.fee, 0);
  EXPECT_EQ(tx.isCoinbase, false);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, getTransactionFailure) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::WalletLegacyTransaction tx;

  ASSERT_EQ(alice->getTransactionCount(), 0);
  ASSERT_FALSE(alice->getTransaction(0, tx));

  alice->shutdown();
}

TEST_F(WalletLegacyApi, useNotInitializedObject) {
  EXPECT_THROW(alice->pendingBalance(), std::system_error);
  EXPECT_THROW(alice->actualBalance(), std::system_error);
  EXPECT_THROW(alice->getTransactionCount(), std::system_error);
  EXPECT_THROW(alice->getTransferCount(), std::system_error);
  EXPECT_THROW(alice->getAddress(), std::system_error);

  std::stringstream archive;
  EXPECT_THROW(alice->save(archive, true, true), std::system_error);

  EXPECT_THROW(alice->findTransactionByTransferId(1), std::system_error);

  CryptoNote::WalletLegacyTransaction tx;
  CryptoNote::WalletLegacyTransfer tr;
  EXPECT_THROW(alice->getTransaction(1, tx), std::system_error);
  EXPECT_THROW(alice->getTransfer(2, tr), std::system_error);

  tr.address = "lslslslslslsls";
  tr.amount = 1000000;
  EXPECT_THROW(alice->sendTransaction(tr, 300201), std::system_error);

  std::vector<CryptoNote::WalletLegacyTransfer> trs;
  trs.push_back(tr);
  EXPECT_THROW(alice->sendTransaction(trs, 329293), std::system_error);
}

TEST_F(WalletLegacyApi, sendWrongAmount) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::WalletLegacyTransfer tr;
  tr.address = "1234567890qwertasdfgzxcvbyuiophjklnm";
  tr.amount = 1;

  EXPECT_THROW(alice->sendTransaction(tr, 1), std::system_error);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, wrongPassword) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  std::stringstream archive;
  alice->save(archive, true, false);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  alice->shutdown();

  prepareAliceWallet();
  alice->initAndLoad(archive, "wrongpass");

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  EXPECT_EQ(result.value(), CryptoNote::error::WRONG_PASSWORD);
}

TEST_F(WalletLegacyApi, detachBlockchain) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  aliceNode->startAlternativeChain(3);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(0, alice->actualBalance());
  EXPECT_EQ(0, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, saveAndLoad) {
  alice->initAndGenerate("pass");

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);

  std::stringstream archive;
  ASSERT_NO_FATAL_FAILURE(alice->save(archive));

  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);
}

TEST_F(WalletLegacyApi, DISABLED_saveAndLoadErroneousTxsCacheDetails) {
  prepareBobWallet();
  prepareCarolWallet();

  std::error_code result;

  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  carol->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  std::array<int64_t, 5> amounts;
  amounts[0] = 1234567;
  amounts[1] = 1345678;
  amounts[2] = 1456789;
  amounts[3] = 1567890;
  amounts[4] = 1678901;
  uint64_t fee = 10000;

  ASSERT_NO_FATAL_FAILURE(performTransferWithErrorTx(amounts, fee));

  std::stringstream archive;
  alice->save(archive);

  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);

  EXPECT_EQ(alice->getTransactionCount(), 2);
  EXPECT_EQ(alice->getTransferCount(), 2);

  CryptoNote::WalletLegacyTransaction tx;
  ASSERT_TRUE(alice->getTransaction(1, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amounts[3] + amounts[4] + fee));
  EXPECT_EQ(tx.firstTransferId, 0);
  EXPECT_EQ(tx.transferCount, 2);

  CryptoNote::WalletLegacyTransfer tr;
  ASSERT_TRUE(alice->getTransfer(0, tr));
  EXPECT_EQ(tr.amount, amounts[3]);
  EXPECT_EQ(tr.address, bob->getAddress());

  ASSERT_TRUE(alice->getTransfer(1, tr));
  EXPECT_EQ(tr.amount, amounts[4]);
  EXPECT_EQ(tr.address, carol->getAddress());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, DISABLED_saveAndLoadErroneousTxsCacheNoDetails) {
  prepareBobWallet();
  prepareCarolWallet();

  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  carol->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  std::array<int64_t, 5> amounts;
  amounts[0] = 1234567;
  amounts[1] = 1345678;
  amounts[2] = 1456789;
  amounts[3] = 1567890;
  amounts[4] = 1678901;
  uint64_t fee = 10000;

  ASSERT_NO_FATAL_FAILURE(performTransferWithErrorTx(amounts, fee));

  std::stringstream archive;
  alice->save(archive, false, true);

  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);

  EXPECT_EQ(0, alice->getTransactionCount());
  EXPECT_EQ(0, alice->getTransferCount());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, mineSaveNoCacheNoDetailsRefresh) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getBlockRewardForAddress(address);
  generator.getBlockRewardForAddress(address);
  generator.getBlockRewardForAddress(address);

  aliceNode->updateObservers();

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  std::stringstream archive;
  alice->save(archive, false, false);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  alice->shutdown();

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD * 3, alice->pendingBalance());
  alice->shutdown();
}


TEST_F(WalletLegacyApi, sendMoneyToMyself) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getBlockRewardForAddress(address);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::TransactionId txId = TransferMoney(*alice, *alice, 100000000, 100);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD - 100, alice->actualBalance());
  ASSERT_EQ(0, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, sendSeveralTransactions) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  for (int i = 0; i < 5; ++i) {
    GetOneBlockReward(*alice);
  }

  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  auto aliceBalance = alice->actualBalance();

  uint64_t sendAmount = 100000;
  uint64_t totalSentAmount = 0;
  size_t transactionCount = 0;
  
  for (int i = 0; i < 10 && alice->actualBalance() > sendAmount; ++i) {
    CryptoNote::WalletLegacyTransfer tr;
    tr.address = bob->getAddress();
    tr.amount = sendAmount;

    auto txId = alice->sendTransaction(tr, m_currency.minimumFee(), "", 1, 0);  
    ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);

    std::error_code sendResult;
    ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), sendResult));
    ASSERT_EQ(std::error_code(), sendResult);

    ++transactionCount;
    totalSentAmount += sendAmount;
  }

  generator.generateEmptyBlocks(10);

  bobNode->updateObservers();

  while (totalSentAmount != bob->actualBalance()) {
    ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));
  }

  EXPECT_EQ(transactionCount, bob->getTransactionCount());
  EXPECT_EQ(0, bob->pendingBalance());
  EXPECT_EQ(totalSentAmount, bob->actualBalance());

  uint64_t aliceTotalBalance = alice->actualBalance() + alice->pendingBalance();
  EXPECT_EQ(aliceBalance - transactionCount * (sendAmount + m_currency.minimumFee()), aliceTotalBalance);
}

TEST_F(WalletLegacyApi, balanceAfterFailedTransaction) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  auto actualBalance = alice->actualBalance();
  auto pendingBalance = alice->pendingBalance();

  uint64_t send = 11000000;
  uint64_t fee = m_currency.minimumFee();

  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = send;

  aliceNode->setNextTransactionError();

  alice->sendTransaction(tr, fee, "", 1, 0);
  generator.generateEmptyBlocks(1);

  ASSERT_EQ(actualBalance, alice->actualBalance());
  ASSERT_EQ(pendingBalance, alice->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, checkPendingBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  uint64_t startActualBalance = alice->actualBalance();
  int64_t sendAmount = 304050;
  uint64_t fee = m_currency.minimumFee();

  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = sendAmount;

  auto txId = alice->sendTransaction(tr, fee, "", 1, 0);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);

  std::error_code sendResult;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), sendResult));
  ASSERT_EQ(std::error_code(), sendResult);

  uint64_t totalBalance = alice->actualBalance() + alice->pendingBalance();
  ASSERT_EQ(startActualBalance - sendAmount - fee, totalBalance);

  generator.generateEmptyBlocks(6);
  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  ASSERT_EQ(sendAmount, bob->actualBalance());
  ASSERT_EQ(0, bob->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, checkChange) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t banknote = 1000000000;
  uint64_t sendAmount = 50000;
  uint64_t fee = m_currency.minimumFee();

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = sendAmount;

  auto txId = alice->sendTransaction(tr, fee, "", 1, 0);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);

  std::error_code sendResult;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), sendResult));
  ASSERT_EQ(std::error_code(), sendResult);

  EXPECT_EQ(0, alice->actualBalance());
  EXPECT_EQ(banknote - sendAmount - fee, alice->pendingBalance());
}

TEST_F(WalletLegacyApi, checkBalanceAfterSend) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  uint64_t banknote = 1000000000;

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));

  //Once wallet takes outputs in random fashion we don't know for sure which outputs will be taken.
  //In this case we generate controllable set of outs.
  generator.getSingleOutputTransaction(address, banknote);
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  const uint64_t sendAmount = 10000000;
  const uint64_t fee = 100;
  CryptoNote::TransactionId txId = TransferMoney(*alice, *alice, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  ASSERT_EQ(banknote, alice->actualBalance());
  ASSERT_EQ(banknote - sendAmount - fee, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, moneyInPoolDontAffectActualBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t banknote = 1000000000;

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  const uint64_t sendAmount = 10000000;
  const uint64_t fee = 100;
  aliceNode->setNextTransactionToPool();
  CryptoNote::TransactionId txId = TransferMoney(*alice, *bob, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(0, alice->actualBalance());
  EXPECT_EQ(banknote - sendAmount - fee, alice->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, balanceAfterTransactionsPlacedInBlockchain) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t banknote = 1000000000;

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  const uint64_t sendAmount = 10000000;
  const uint64_t fee = 100;
  aliceNode->setNextTransactionToPool();
  CryptoNote::TransactionId txId = TransferMoney(*alice, *bob, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  aliceNode->includeTransactionsFromPoolToBlock();
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(banknote - sendAmount - fee, alice->actualBalance());
  EXPECT_EQ(0, alice->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, checkMyMoneyInTxPool) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  uint64_t sendAmount = 8821902;
  uint64_t fee = 10000;

  aliceNode->setNextTransactionToPool();
  CryptoNote::TransactionId txId = TransferMoney(*alice, *bob, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  EXPECT_EQ(0, bob->actualBalance());
  EXPECT_EQ(sendAmount, bob->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, initWithKeys) {
  CryptoNote::AccountKeys accountKeys;

  Crypto::generate_keys(accountKeys.address.spendPublicKey, accountKeys.spendSecretKey);
  Crypto::generate_keys(accountKeys.address.viewPublicKey, accountKeys.viewSecretKey);

  alice->initWithKeys(accountKeys, "pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get()));

  CryptoNote::AccountKeys keys;
  alice->getAccountKeys(keys);
  
  EXPECT_EQ(accountKeys.address.spendPublicKey, keys.address.spendPublicKey);
  EXPECT_EQ(accountKeys.spendSecretKey, keys.spendSecretKey);
  EXPECT_EQ(accountKeys.address.viewPublicKey, keys.address.viewPublicKey);
  EXPECT_EQ(accountKeys.viewSecretKey, keys.viewSecretKey);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, deleteTxFromPool) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  uint64_t sendAmount = 9748291;
  uint64_t fee = 10000;

  aliceNode->setNextTransactionToPool();
  CryptoNote::TransactionId txId = TransferMoney(*alice, *bob, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  alice->shutdown();

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  generator.clearTxPool();

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  EXPECT_EQ(0, bob->actualBalance());
  EXPECT_EQ(0, bob->pendingBalance());

  bob->shutdown();
}

TEST_F(WalletLegacyApi, sendAfterFailedTransaction) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::WalletLegacyTransfer tr;
  tr.amount = 100000;
  tr.address = "wrong_address";

  EXPECT_THROW(alice->sendTransaction(tr, 1000, "", 2, 0), std::system_error);
  CryptoNote::TransactionId txId = TransferMoney(*alice, *alice, 100000, 100);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  alice->shutdown();
}

TEST_F(WalletLegacyApi, DISABLED_loadingBrokenCache) {
  alice->initAndGenerate("pass");

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  ASSERT_EQ(result.value(), 0);

  std::stringstream archive;
  ASSERT_NO_FATAL_FAILURE(alice->save(archive, false, true));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  size_t sizeWithEmptyCache = archive.str().size();

  for (size_t i = 0; i < 3; ++i) {
    GetOneBlockReward(*alice);
  }
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  archive.str("");
  archive.clear();

  ASSERT_NO_FATAL_FAILURE(alice->save(archive, false, true));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  std::string state = archive.str();
  for (size_t i = sizeWithEmptyCache; i < state.size(); ++i) {
    state[i] = '\xff';
  }
  archive.str(state);

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);
}

TEST_F(WalletLegacyApi, outcommingExternalTransactionTotalAmount) {
  class ExternalTxChecker : public CryptoNote::IWalletLegacyObserver {
  public:
    ExternalTxChecker(CryptoNote::WalletLegacy& wallet) : wallet(wallet), totalAmount(std::numeric_limits<int64_t>::max()) {
    }

    virtual void externalTransactionCreated(CryptoNote::TransactionId transactionId) override {
      CryptoNote::WalletLegacyTransaction txInfo;
      ASSERT_TRUE(wallet.getTransaction(transactionId, txInfo));
      totalAmount = txInfo.totalAmount;
    }

    CryptoNote::WalletLegacy& wallet;
    int64_t totalAmount;
  };

  std::stringstream walletData;

  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  alice->save(walletData, false, false);
  WaitWalletSave(aliceWalletObserver.get());

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());

  prepareBobWallet();
  bob->initAndGenerate("pass2");
  WaitWalletSync(bobWalletObserver.get());

  uint64_t sent = 10000000;
  uint64_t fee = 1000;

  CryptoNote::WalletLegacyTransfer tr;
  tr.amount = sent;
  tr.address = bob->getAddress();

  alice->sendTransaction(tr, fee);
  WaitWalletSend(aliceWalletObserver.get());

  bob->shutdown();
  alice->shutdown();

  CryptoNote::WalletLegacy wallet(m_currency, *aliceNode);

  ExternalTxChecker externalTransactionObserver(wallet);
  TrivialWalletObserver walletObserver;

  wallet.addObserver(&externalTransactionObserver);
  wallet.addObserver(&walletObserver);

  wallet.initAndLoad(walletData, "pass");
  WaitWalletSync(&walletObserver);

  ASSERT_EQ(-static_cast<int64_t>(sent + fee), externalTransactionObserver.totalAmount);
  wallet.shutdown();
}

TEST_F(WalletLegacyApi, shutdownAllowsInitializeWalletWithTheSameKeys) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::AccountKeys accountKeys;
  alice->getAccountKeys(accountKeys);

  alice->shutdown();
  alice->initWithKeys(accountKeys, "pass");

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(1, alice->getTransactionCount());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, shutdownAllowsInitializeWalletWithDifferentKeys) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->shutdown();
  alice->initAndGenerate("pass");

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(0, alice->getTransactionCount());

  alice->shutdown();
}

namespace {
class WalletSynchronizationProgressUpdatedObserver : public CryptoNote::IWalletLegacyObserver {
public:
  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t /*total*/) override {
    m_current = current;
  }

  uint64_t m_current = 0;
};
}

TEST_F(WalletLegacyApi, shutdownDoesNotRemoveObservers) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  WalletSynchronizationProgressUpdatedObserver observer;
  CryptoNote::WalletHelper::IWalletRemoveObserverGuard observerGuard(*alice, observer);

  alice->shutdown();
  observer.m_current = 0;
  alice->initAndGenerate("pass");

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(5, observer.m_current);

  observerGuard.removeObserver();
  alice->shutdown();
}

namespace {
class WalletTransactionEventCounter : public CryptoNote::IWalletLegacyObserver {
public:
  virtual void externalTransactionCreated(CryptoNote::TransactionId /*transactionId*/) override {
    ++m_count;
  }

  virtual void transactionUpdated(CryptoNote::TransactionId /*transactionId*/) override {
    ++m_count;
  }

  size_t m_count = 0;
};
}

TEST_F(WalletLegacyApi, afterShutdownAndInitWalletDoesNotSendNotificationsRelatedToOldAddress) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  std::string aliceAddress1 = alice->getAddress();
  CryptoNote::AccountKeys accountKeys1;
  alice->getAccountKeys(accountKeys1);

  alice->shutdown();
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  std::string aliceAddress2 = alice->getAddress();

  alice->shutdown();
  alice->initWithKeys(accountKeys1, "pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  WalletTransactionEventCounter observer;
  CryptoNote::WalletHelper::IWalletRemoveObserverGuard observerGuard(*alice, observer);

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));
  GetOneBlockReward(*bob);
  generator.generateEmptyBlocks(10);
  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  std::vector<CryptoNote::WalletLegacyTransfer> transfers;
  transfers.push_back({ aliceAddress1, TEST_BLOCK_REWARD / 10 });
  transfers.push_back({ aliceAddress2, TEST_BLOCK_REWARD / 5 });
  bob->sendTransaction(transfers, m_currency.minimumFee());
  std::error_code sendResult;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(bobWalletObserver.get(), sendResult));

  generator.generateEmptyBlocks(1);
  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(1, observer.m_count);

  observerGuard.removeObserver();
  bob->shutdown();
  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetDoesNotChangeAddress) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  auto expectedAddress = alice->getAddress();
  alice->reset();
  ASSERT_EQ(expectedAddress, alice->getAddress());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetDoesNotChangeAccountKeys) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::AccountKeys expectedAccountKeys;
  alice->getAccountKeys(expectedAccountKeys);

  alice->reset();

  CryptoNote::AccountKeys actualAccountKeys;
  alice->getAccountKeys(actualAccountKeys);

  ASSERT_EQ(expectedAccountKeys.address, actualAccountKeys.address);
  ASSERT_EQ(expectedAccountKeys.spendSecretKey, actualAccountKeys.spendSecretKey);
  ASSERT_EQ(expectedAccountKeys.viewSecretKey, actualAccountKeys.viewSecretKey);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetDoesNotRemoveObservers) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  WalletSynchronizationProgressUpdatedObserver observer;
  CryptoNote::WalletHelper::IWalletRemoveObserverGuard observerGuard(*alice, observer);

  alice->reset();
  observer.m_current = 0;

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(5, observer.m_current);

  observerGuard.removeObserver();
  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetDoesNotChangePassword) {
  std::string password = "password";
  std::string newPassword = "new_password";

  alice->initAndGenerate(password);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->reset();
  ASSERT_TRUE(static_cast<bool>(alice->changePassword(newPassword, password)));
  ASSERT_FALSE(static_cast<bool>(alice->changePassword(password, newPassword)));

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetClearsPendingBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD, alice->pendingBalance());
  alice->reset();
  ASSERT_EQ(0, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetClearsActualBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD, alice->actualBalance());
  alice->reset();
  ASSERT_EQ(0, alice->actualBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetClearsTransactionHistory) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(1, alice->getTransactionCount());
  alice->reset();
  ASSERT_EQ(0, alice->getTransactionCount());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetClearsTransfersHistory) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->sendTransaction({ alice->getAddress(), 100 }, m_currency.minimumFee());
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  ASSERT_EQ(1, alice->getTransferCount());
  alice->reset();
  ASSERT_EQ(0, alice->getTransferCount());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetAndSyncRestorePendingBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->reset();
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetAndSyncRestoreActualBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->reset();
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD, alice->actualBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetAndSyncRestoreTransactions) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->reset();
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(1, alice->getTransactionCount());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetAndSyncDoNotRestoreTransfers) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->sendTransaction({ alice->getAddress(), 100 }, m_currency.minimumFee());
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  alice->reset();
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(0, alice->getTransferCount());

  alice->shutdown();
}

void generateWallet(CryptoNote::IWalletLegacy& wallet, TrivialWalletObserver& observer, const std::string& pass) {
  wallet.initAndGenerate(pass);
  WaitWalletSync(&observer);
}

TEST_F(WalletLegacyApi, outdatedUnconfirmedTransactionDeletedOnNewBlock) {
  const uint64_t TRANSACTION_MEMPOOL_TIME = 1;
  CryptoNote::Currency currency(CryptoNote::CurrencyBuilder(m_logger).mempoolTxLiveTime(TRANSACTION_MEMPOOL_TIME).currency());
  TestBlockchainGenerator blockchainGenerator(currency);
  INodeTrivialRefreshStub node(blockchainGenerator);
  CryptoNote::WalletLegacy wallet(currency, node);
  TrivialWalletObserver walletObserver;
  wallet.addObserver(&walletObserver);

  wallet.initAndGenerate("pass");
  WaitWalletSync(&walletObserver);

  GetOneBlockRewardAndUnlock(wallet, walletObserver, node, currency, blockchainGenerator);

  CryptoNote::AccountBase account;
  account.generate();
  const std::string ADDRESS = currency.accountAddressAsString(account.getAccountKeys().address);
  node.setNextTransactionToPool();
  auto id = wallet.sendTransaction({ADDRESS, static_cast<int64_t>(TEST_BLOCK_REWARD - m_currency.minimumFee())}, m_currency.minimumFee());
  WaitWalletSend(&walletObserver);

  node.cleanTransactionPool();
  std::this_thread::sleep_for(std::chrono::seconds(TRANSACTION_MEMPOOL_TIME));

  blockchainGenerator.generateEmptyBlocks(1);
  node.updateObservers();
  WaitWalletSync(&walletObserver);

  ASSERT_EQ(TEST_BLOCK_REWARD, wallet.actualBalance());

  CryptoNote::WalletLegacyTransaction transaction;
  ASSERT_TRUE(wallet.getTransaction(id, transaction));
  EXPECT_EQ(CryptoNote::WalletLegacyTransactionState::Deleted, transaction.state);

  wallet.removeObserver(&walletObserver);
  wallet.shutdown();
}

TEST_F(WalletLegacyApi, outdatedUnconfirmedTransactionDeletedOnLoad) {
  const uint64_t TRANSACTION_MEMPOOL_TIME = 1;
  CryptoNote::Currency currency(CryptoNote::CurrencyBuilder(m_logger).mempoolTxLiveTime(TRANSACTION_MEMPOOL_TIME).currency());
  TestBlockchainGenerator blockchainGenerator(currency);
  INodeTrivialRefreshStub node(blockchainGenerator);
  CryptoNote::WalletLegacy wallet(currency, node);
  TrivialWalletObserver walletObserver;
  wallet.addObserver(&walletObserver);

  wallet.initAndGenerate("pass");
  WaitWalletSync(&walletObserver);

  GetOneBlockRewardAndUnlock(wallet, walletObserver, node, currency, blockchainGenerator);

  CryptoNote::AccountBase account;
  account.generate();
  const std::string ADDRESS = currency.accountAddressAsString(account.getAccountKeys().address);
  node.setNextTransactionToPool();
  auto id = wallet.sendTransaction({ADDRESS, static_cast<int64_t>(TEST_BLOCK_REWARD - m_currency.minimumFee())}, m_currency.minimumFee());
  WaitWalletSend(&walletObserver);

  node.cleanTransactionPool();

  std::stringstream data;
  wallet.save(data);
  WaitWalletSave(&walletObserver);

  wallet.shutdown();

  std::this_thread::sleep_for(std::chrono::seconds(TRANSACTION_MEMPOOL_TIME));

  wallet.initAndLoad(data, "pass");
  WaitWalletSync(&walletObserver);

  ASSERT_EQ(TEST_BLOCK_REWARD, wallet.actualBalance());

  CryptoNote::WalletLegacyTransaction transaction;
  ASSERT_TRUE(wallet.getTransaction(id, transaction));
  EXPECT_EQ(CryptoNote::WalletLegacyTransactionState::Deleted, transaction.state);

  wallet.removeObserver(&walletObserver);
  wallet.shutdown();
}

TEST_F(WalletLegacyApi, walletLoadsNullSpendSecretKey) {
  CryptoNote::AccountKeys accountKeys;

  Crypto::generate_keys(accountKeys.address.spendPublicKey, accountKeys.spendSecretKey);
  Crypto::generate_keys(accountKeys.address.viewPublicKey, accountKeys.viewSecretKey);
  accountKeys.spendSecretKey = CryptoNote::NULL_SECRET_KEY;

  alice->initWithKeys(accountKeys, "pass");
  WaitWalletSync(aliceWalletObserver.get());

  std::stringstream data;
  alice->save(data);
  WaitWalletSave(aliceWalletObserver.get());

  alice->shutdown();

  alice->initAndLoad(data, "pass");
  WaitWalletSync(aliceWalletObserver.get());

  ASSERT_EQ(std::error_code(), aliceWalletObserver->loadResult);
  alice->shutdown();
}

TEST_F(WalletLegacyApi, sendMessage) {
  prepareBobWallet();

  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GenerateOneBlockRewardAndUnlock();

  bob->initAndGenerate("pass2");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  std::string text = "darkwing duck!";
  std::vector<CryptoNote::TransactionMessage> messages;
  messages.push_back({ text, bob->getAddress() });
  TransferMoney(*alice, *bob, 100, 10, 0, std::string(), messages);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  generator.generateEmptyBlocks(1);
  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  ASSERT_EQ(1, bob->getTransactionCount());
  CryptoNote::WalletLegacyTransaction tx;
  ASSERT_TRUE(bob->getTransaction(0, tx));
  ASSERT_EQ(1, tx.messages.size());
  ASSERT_EQ(text, tx.messages[0]);

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, sendBulkOfMessages) {
  prepareBobWallet();
  prepareCarolWallet();

  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GenerateOneBlockRewardAndUnlock();

  bob->initAndGenerate("pass2");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  carol->initAndGenerate("pass3");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  std::string verse1 = "Daring duck of mystery, \n"
    "Champion of right, \n"
    "Swoops out of the shadows, \n"
    "Darkwing owns the night. \n"
    "Somewhere some villain schemes, \n"
    "But his number's up. \n"
    "source: http://www.lyricsondemand.com/";

  std::string chorus = "(3-2-1) Darkwing Duck (When there's trouble you call DW) \n"
    "Darkwing Duck (Let's get dangerous) \n"
    "Darkwing Duck (Darkwing, Darkwing Duck!) \n"
    "source: http://www.lyricsondemand.com/";

  std::string verse2 = "Cloud of smoke and he appears, \n"
    "Master of surprise. \n"
    "Who's that cunning mind behind \n"
    "That shadowy disguise? \n"
    "Nobody knows for sure, \n"
    "But bad guys are out of luck. \n"
    "source: http://www.lyricsondemand.com/";

  std::string verse3 = "'Cause here comes (Darkwing Duck) \n"
    "Look out! (When there's trouble you call DW) \n"
    "Darkwing Duck (Let's get dangerous) \n"
    "Darkwing Duck (Better watch out, you bad boys) \n"
    "Darkwing Duck!\n"
    "source: http://www.lyricsondemand.com/";

  std::vector<CryptoNote::TransactionMessage> messages;
  messages.push_back({ verse1, bob->getAddress() });
  messages.push_back({ chorus, bob->getAddress() });
  messages.push_back({ verse2, bob->getAddress() });
  messages.push_back({ verse3, bob->getAddress() });

  std::vector<CryptoNote::WalletLegacyTransfer> transfers;
  transfers.push_back({ bob->getAddress(), 100 });
  transfers.push_back({ carol->getAddress(), 100 });

  alice->sendTransaction(transfers, 10, std::string(), 0, 0, messages);

  generator.generateEmptyBlocks(1);
  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  carolNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  CryptoNote::WalletLegacyTransaction bobTx;
  ASSERT_EQ(1, bob->getTransactionCount());
  ASSERT_TRUE(bob->getTransaction(0, bobTx));
  ASSERT_EQ(4, bobTx.messages.size());
  //there's no guarantee of any particular order
  ASSERT_NE(bobTx.messages.end(), std::find(bobTx.messages.begin(), bobTx.messages.end(), verse1));
  ASSERT_NE(bobTx.messages.end(), std::find(bobTx.messages.begin(), bobTx.messages.end(), chorus));
  ASSERT_NE(bobTx.messages.end(), std::find(bobTx.messages.begin(), bobTx.messages.end(), verse2));
  ASSERT_NE(bobTx.messages.end(), std::find(bobTx.messages.begin(), bobTx.messages.end(), verse3));

  CryptoNote::WalletLegacyTransaction carolTx;
  ASSERT_EQ(1, carol->getTransactionCount());
  ASSERT_TRUE(carol->getTransaction(0, carolTx));
  ASSERT_EQ(0, carolTx.messages.size());

  alice->shutdown();
  bob->shutdown();
  carol->shutdown();
}

TEST_F(WalletLegacyApi, depositReturnsCorrectDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();
  const uint64_t AMOUNT = m_currency.depositMinAmount();

  auto txId = alice->deposit(TERM, AMOUNT, FEE);
  WaitWalletSend(aliceWalletObserver.get());

  CryptoNote::WalletLegacyTransaction info;
  ASSERT_TRUE(alice->getTransaction(txId, info));

  EXPECT_EQ(0, info.firstDepositId);
  EXPECT_EQ(1, info.depositCount);
  EXPECT_EQ(-static_cast<int64_t>(AMOUNT + FEE), info.totalAmount);
  EXPECT_EQ(CryptoNote::WALLET_LEGACY_INVALID_TRANSFER_ID, info.firstTransferId);
  EXPECT_EQ(0, info.transferCount);
  EXPECT_EQ(FEE, info.fee);

  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(0, deposit));
  EXPECT_EQ(txId, deposit.creatingTransactionId);
  EXPECT_EQ(CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID, deposit.spendingTransactionId);
  EXPECT_EQ(TERM, deposit.term);
  EXPECT_EQ(AMOUNT, deposit.amount);
  EXPECT_EQ(m_currency.calculateInterest(deposit.amount, deposit.term), deposit.interest);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositWithMixinReturnsCorrectDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t FEE = m_currency.minimumFee();

  auto txId = alice->deposit(TERM, AMOUNT, FEE, 3);
  WaitWalletSend(aliceWalletObserver.get());

  CryptoNote::WalletLegacyTransaction info;
  ASSERT_TRUE(alice->getTransaction(txId, info));

  EXPECT_EQ(0, info.firstDepositId);
  EXPECT_EQ(1, info.depositCount);
  EXPECT_EQ(-static_cast<int64_t>(AMOUNT + FEE), info.totalAmount);
  EXPECT_EQ(CryptoNote::WALLET_LEGACY_INVALID_TRANSFER_ID, info.firstTransferId);
  EXPECT_EQ(0, info.transferCount);
  EXPECT_EQ(FEE, info.fee);

  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(0, deposit));
  EXPECT_EQ(txId, deposit.creatingTransactionId);
  EXPECT_EQ(CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID, deposit.spendingTransactionId);
  EXPECT_EQ(TERM, deposit.term);
  EXPECT_EQ(AMOUNT, deposit.amount);
  EXPECT_EQ(m_currency.calculateInterest(deposit.amount, deposit.term), deposit.interest);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsUpdatedCallbackCame) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  alice->deposit(m_currency.depositMinTerm(), m_currency.depositMinAmount(), m_currency.minimumFee(), 3);
  ASSERT_TRUE(aliceWalletObserver->waitForDepositsUpdated());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsRestoredAfterSerialization) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT1 = m_currency.depositMinAmount();
  const uint64_t AMOUNT2 = m_currency.depositMinAmount() + 992;
  const uint32_t TERM1 = m_currency.depositMinTerm();
  const uint32_t TERM2 = m_currency.depositMinTerm() + 1;

  auto firstTx = alice->deposit(TERM1, AMOUNT1, m_currency.minimumFee());
  WaitWalletSend(aliceWalletObserver.get());

  auto secondTx = alice->deposit(TERM2, AMOUNT2, m_currency.minimumFee());
  WaitWalletSend(aliceWalletObserver.get());

  std::stringstream data;
  alice->save(data, false, false);
  WaitWalletSave(aliceWalletObserver.get());
  alice->shutdown();

  prepareBobWallet();
  bob->initAndLoad(data, "pass");
  WaitWalletSync(bobWalletObserver.get());

  ASSERT_EQ(2, bob->getDepositCount());

  CryptoNote::Deposit deposit1;
  ASSERT_TRUE(bob->getDeposit(0, deposit1));
  EXPECT_EQ(AMOUNT1, deposit1.amount);
  EXPECT_EQ(TERM1, deposit1.term);
  EXPECT_EQ(firstTx, deposit1.creatingTransactionId);
  EXPECT_EQ(CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID, deposit1.spendingTransactionId);
  EXPECT_EQ(m_currency.calculateInterest(deposit1.amount, deposit1.term), deposit1.interest);

  CryptoNote::Deposit deposit2;
  ASSERT_TRUE(bob->getDeposit(1, deposit2));
  EXPECT_EQ(AMOUNT2, deposit2.amount);
  EXPECT_EQ(TERM2, deposit2.term);
  EXPECT_EQ(secondTx, deposit2.creatingTransactionId);
  EXPECT_EQ(CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID, deposit2.spendingTransactionId);
  EXPECT_EQ(m_currency.calculateInterest(deposit2.amount, deposit2.term), deposit2.interest);

  bob->shutdown();
}

TEST_F(WalletLegacyApi, depositsRestoredFromBlockchain) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t AMOUNT2 = m_currency.depositMinAmount() + 1;
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto unlockedDepositId = makeDepositAndUnlock(AMOUNT, TERM, FEE);
  auto unlockedDepositCreatingTransactionId = alice->getTransactionCount() - 1;

  auto lockedDepositId = makeDeposit(AMOUNT2, TERM, FEE);
  auto lockedDepositCreatingTransactionId = alice->getTransactionCount() - 1;

  std::stringstream data;
  alice->save(data, false, false);
  WaitWalletSave(aliceWalletObserver.get());

  alice->shutdown();

  prepareBobWallet();
  bob->initAndLoad(data, "pass");
  WaitWalletSync(bobWalletObserver.get());

  ASSERT_EQ(2, bob->getDepositCount());

  CryptoNote::Deposit unlockedDeposit;
  bob->getDeposit(unlockedDepositId, unlockedDeposit);
  EXPECT_EQ(AMOUNT, unlockedDeposit.amount);
  EXPECT_EQ(TERM, unlockedDeposit.term);
  EXPECT_EQ(m_currency.calculateInterest(AMOUNT, TERM), unlockedDeposit.interest);
  EXPECT_EQ(unlockedDepositCreatingTransactionId, unlockedDeposit.creatingTransactionId);
  EXPECT_EQ(CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID, unlockedDeposit.spendingTransactionId);
  EXPECT_FALSE(unlockedDeposit.locked);

  CryptoNote::Deposit lockedDeposit;
  bob->getDeposit(lockedDepositId, lockedDeposit);
  EXPECT_EQ(AMOUNT2, lockedDeposit.amount);
  EXPECT_EQ(TERM, lockedDeposit.term);
  EXPECT_EQ(m_currency.calculateInterest(AMOUNT2, TERM), lockedDeposit.interest);
  EXPECT_EQ(lockedDepositCreatingTransactionId, lockedDeposit.creatingTransactionId);
  EXPECT_EQ(CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID, lockedDeposit.spendingTransactionId);
  EXPECT_TRUE(lockedDeposit.locked);

  bob->shutdown();
}

TEST_F(WalletLegacyApi, depositsUnlock) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto walletActualBalance = alice->actualBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto depositId = makeDepositAndUnlock(AMOUNT, TERM, FEE);

  uint64_t expectedActualDepositBalance = calculateTotalDepositAmount(AMOUNT, TERM);
  EXPECT_EQ(expectedActualDepositBalance, alice->actualDepositBalance());
  EXPECT_EQ(0, alice->pendingDepositBalance());

  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(depositId, deposit));
  EXPECT_FALSE(deposit.locked);

  EXPECT_EQ(walletActualBalance - AMOUNT - FEE, alice->actualBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsWithTooSmallTerm) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm() - 1;
  const uint64_t FEE = m_currency.minimumFee();

  ASSERT_ANY_THROW(makeDeposit(AMOUNT, TERM, FEE));
  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsWithTooBigTerm) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMaxTerm() + 1;
  const uint64_t FEE = m_currency.minimumFee();

  ASSERT_ANY_THROW(makeDeposit(AMOUNT, TERM, FEE));
  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsWithTooSmallAmount) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount() - 1;
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  ASSERT_ANY_THROW(makeDeposit(AMOUNT, TERM, FEE));
  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsUpdatedCallbackCalledOnDepositUnlock) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto depositId = makeDeposit(AMOUNT, TERM, FEE);

  DepositsUpdatedScopedObserver depositsUpdatedWaiter(*alice);

  unlockDeposit(TERM);

  auto depositsUpdated = depositsUpdatedWaiter.wait();
  ASSERT_EQ(1, depositsUpdated.size());
  EXPECT_EQ(depositId, depositsUpdated[0]);

  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(depositId, deposit));
  EXPECT_FALSE(deposit.locked);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsWithdraw) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();
  const uint64_t FEE2 = m_currency.minimumFee();

  auto id = makeDepositAndUnlock(AMOUNT, TERM, FEE);

  withdrawDeposits({id}, FEE2);
  EXPECT_EQ(calculateTotalDepositAmount(AMOUNT, TERM) - FEE2, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsCheckSpendingTransactionId) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto id = makeDepositAndUnlock(AMOUNT, TERM, FEE);
  auto spendingTxId = withdrawDeposits({id}, FEE);

  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(id, deposit));
  EXPECT_EQ(spendingTxId, deposit.spendingTransactionId);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsWithdrawTwoDepositsCheckSpendingTransactionId) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t AMOUNT2 = m_currency.depositMinAmount() + 1;
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto depositId1 = makeDeposit(AMOUNT, TERM, FEE);
  auto depositId2 = makeDeposit(AMOUNT2, TERM, FEE);

  unlockDeposit(TERM);

  auto spendingTxId = withdrawDeposits({depositId1, depositId2}, FEE);

  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(depositId1, deposit));
  EXPECT_EQ(spendingTxId, deposit.spendingTransactionId);

  CryptoNote::Deposit deposit2;
  ASSERT_TRUE(alice->getDeposit(depositId2, deposit2));
  EXPECT_EQ(spendingTxId, deposit2.spendingTransactionId);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsWithdrawWrongDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  ASSERT_ANY_THROW(withdrawDeposits({3}, m_currency.minimumFee()));

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsWithdrawLockedDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto depositId = makeDeposit(AMOUNT, TERM, FEE);
  unlockDeposit(TERM - 1);

  ASSERT_ANY_THROW(withdrawDeposits({depositId}, FEE));

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsWithdrawFeeGreaterThenAmount) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto depositId = makeDeposit(AMOUNT, TERM, FEE);
  unlockDeposit(TERM);

  ASSERT_ANY_THROW(withdrawDeposits({depositId}, calculateTotalDepositAmount(AMOUNT, TERM) + 1));

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsUpdatedCallbackCalledOnWithdraw) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t AMOUNT2 = m_currency.depositMinAmount() + 1;
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto depositId1 = makeDeposit(AMOUNT, TERM, FEE);
  auto depositId2 = makeDeposit(AMOUNT2, TERM, FEE);

  unlockDeposit(TERM);

  DepositsUpdatedScopedObserver depoUpdated(*alice);

  withdrawDeposits({depositId1, depositId2}, FEE);

  auto updatedDeposits = depoUpdated.wait();
  ASSERT_EQ(2, updatedDeposits.size());
  EXPECT_NE(updatedDeposits.end(), std::find(updatedDeposits.begin(), updatedDeposits.end(), depositId1));
  EXPECT_NE(updatedDeposits.end(), std::find(updatedDeposits.begin(), updatedDeposits.end(), depositId2));

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsBalancesRightAfterMakingDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto initialActualBalance = alice->actualBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  DepositsPendingBalanceChangedScopedObserver depositPendingBalanceChanged(*alice);

  alice->deposit(TERM, AMOUNT, FEE);
  WaitWalletSend(aliceWalletObserver.get());

  auto depositPending = depositPendingBalanceChanged.wait();

  EXPECT_EQ(calculateTotalDepositAmount(AMOUNT, TERM), depositPending);
  EXPECT_EQ(0, alice->actualDepositBalance());

  EXPECT_EQ(initialActualBalance - AMOUNT - FEE, alice->actualBalance() + alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsBalancesAfterUnlockingDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto initialTotalBalance = alice->actualBalance() + alice->pendingBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  makeDeposit(AMOUNT, TERM, FEE);

  DepositsPendingBalanceChangedScopedObserver depositPendingBalanceChanged(*alice);
  DepositsActualBalanceChangedScopedObserver depositActualBalanceChanged(*alice);

  unlockDeposit(TERM);

  auto depositPending = depositPendingBalanceChanged.wait();
  auto depositActual = depositActualBalanceChanged.wait();

  EXPECT_EQ(calculateTotalDepositAmount(AMOUNT, TERM), depositActual);
  EXPECT_EQ(0, depositPending);
  EXPECT_EQ(initialTotalBalance - AMOUNT - FEE,  alice->actualBalance() + alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, depositsBalancesAfterWithdrawDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto initialActualBalance = alice->actualBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();
  const uint64_t FEE2 = m_currency.minimumFee() + 10;

  auto depositId = makeDepositAndUnlock(AMOUNT, TERM, FEE);

  DepositsActualBalanceChangedScopedObserver depositActualBalanceChanged(*alice);
  PendingBalanceChangedScopedObserver pendingBalanceChanged(*alice);

  alice->withdrawDeposits({depositId}, FEE2);

  auto depositActual = depositActualBalanceChanged.wait();
  auto pendingBalance = pendingBalanceChanged.wait();

  EXPECT_EQ(0, depositActual);
  EXPECT_EQ(0, alice->pendingDepositBalance());
  EXPECT_EQ(calculateTotalDepositAmount(AMOUNT, TERM) - FEE2, pendingBalance);
  EXPECT_EQ(initialActualBalance - AMOUNT - FEE,  alice->actualBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, lockedDepositsRemovedAfterDetach) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto initialActualBalance = alice->actualBalance();
  auto initialPendingBalance = alice->pendingBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  uint32_t detachHeight = generator.getCurrentHeight() - 1;

  auto id = makeDeposit(AMOUNT, TERM, FEE, 0);

  DepositsPendingBalanceChangedScopedObserver depositPendingBalanceChanged(*alice);
  DepositsUpdatedScopedObserver depositsUpdatedCalled(*alice);
  ActualBalanceChangedScopedObserver actualBalanceChanged(*alice);

  aliceNode->startAlternativeChain(detachHeight);
  generator.generateEmptyBlocks(1);
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());

  auto depositPendingBalance = depositPendingBalanceChanged.wait();
  auto depositsUpdated = depositsUpdatedCalled.wait();
  auto actualBalance = actualBalanceChanged.wait();

  EXPECT_EQ(initialActualBalance, actualBalance);
  EXPECT_EQ(initialPendingBalance, alice->pendingBalance());
  EXPECT_EQ(0, depositPendingBalance);

  ASSERT_EQ(1, depositsUpdated.size());
  EXPECT_EQ(id, depositsUpdated[0]);

  EXPECT_EQ(1, alice->getDepositCount());
  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(id, deposit));

  CryptoNote::WalletLegacyTransaction txInfo;
  ASSERT_TRUE(alice->getTransaction(deposit.creatingTransactionId, txInfo));

  EXPECT_EQ(CryptoNote::WalletLegacyTransactionState::Deleted, txInfo.state);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, unlockedDepositsRemovedAfterDetach) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto initialActualBalance = alice->actualBalance();
  auto initialPendingBalance = alice->pendingBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto detachHeight = generator.getCurrentHeight() - 1;

  auto id = makeDepositAndUnlock(AMOUNT, TERM, FEE);

  DepositsActualBalanceChangedScopedObserver depositActualBalanceChanged(*alice);
  DepositsUpdatedScopedObserver depositsUpdatedCalled(*alice);
  ActualBalanceChangedScopedObserver actualBalanceChanged(*alice);

  aliceNode->startAlternativeChain(detachHeight);
  generator.generateEmptyBlocks(1);
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());

  auto depositActualBalance = depositActualBalanceChanged.wait();
  auto depositsUpdated = depositsUpdatedCalled.wait();
  auto actualBalance = actualBalanceChanged.wait();

  EXPECT_EQ(initialActualBalance, actualBalance);
  EXPECT_EQ(initialPendingBalance, alice->pendingBalance());
  EXPECT_EQ(0, alice->pendingDepositBalance());
  EXPECT_EQ(0, depositActualBalance);

  ASSERT_EQ(1, depositsUpdated.size());
  EXPECT_EQ(id, depositsUpdated[0]);

  EXPECT_EQ(1, alice->getDepositCount());
  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(id, deposit));

  CryptoNote::WalletLegacyTransaction txInfo;
  ASSERT_TRUE(alice->getTransaction(deposit.creatingTransactionId, txInfo));

  EXPECT_EQ(CryptoNote::WalletLegacyTransactionState::Deleted, txInfo.state);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, unlockedDepositsLockedAfterDetach) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto id = makeDepositAndUnlock(AMOUNT, TERM, FEE);

  auto detachHeight = generator.getCurrentHeight() - 2;

  DepositsActualBalanceChangedScopedObserver depositActualBalanceChanged(*alice);
  DepositsPendingBalanceChangedScopedObserver depositsPendingBalanceChanged(*alice);
  DepositsUpdatedScopedObserver depositsUpdatedCalled(*alice);

  aliceNode->startAlternativeChain(detachHeight);
  generator.generateEmptyBlocks(1);
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());

  auto depositActualBalance = depositActualBalanceChanged.wait();
  auto depositPendingBalance = depositsPendingBalanceChanged.wait();
  auto depositsUpdated = depositsUpdatedCalled.wait();

  EXPECT_EQ(calculateTotalDepositAmount(AMOUNT, TERM), depositPendingBalance);
  EXPECT_EQ(0, depositActualBalance);

  ASSERT_EQ(1, depositsUpdated.size());
  EXPECT_EQ(id, depositsUpdated[0]);

  EXPECT_EQ(1, alice->getDepositCount());
  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(id, deposit));
  EXPECT_TRUE(deposit.locked);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, serializeLockedDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  makeDeposit(AMOUNT, TERM, FEE);

  std::stringstream data;
  alice->save(data);
  WaitWalletSave(aliceWalletObserver.get());

  alice->shutdown();

  prepareBobWallet();
  bob->initAndLoad(data, "pass");
  WaitWalletSync(bobWalletObserver.get());

  ASSERT_EQ(1, bob->getDepositCount());

  CryptoNote::Deposit deposit;
  EXPECT_TRUE(bob->getDeposit(0, deposit));
  EXPECT_EQ(1, deposit.creatingTransactionId);
  EXPECT_EQ(CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID, deposit.spendingTransactionId);
  EXPECT_EQ(TERM, deposit.term);
  EXPECT_EQ(AMOUNT, deposit.amount);
  EXPECT_EQ(m_currency.calculateInterest(AMOUNT, TERM), deposit.interest);
  EXPECT_TRUE(deposit.locked);

  bob->shutdown();
}

TEST_F(WalletLegacyApi, serializeUnlockedDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  makeDepositAndUnlock(AMOUNT, TERM, FEE);

  std::stringstream data;
  alice->save(data);
  WaitWalletSave(aliceWalletObserver.get());

  alice->shutdown();

  prepareBobWallet();
  bob->initAndLoad(data, "pass");
  WaitWalletSync(bobWalletObserver.get());

  ASSERT_EQ(1, bob->getDepositCount());

  CryptoNote::Deposit deposit;
  EXPECT_TRUE(bob->getDeposit(0, deposit));
  EXPECT_EQ(1, deposit.creatingTransactionId);
  EXPECT_EQ(CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID, deposit.spendingTransactionId);
  EXPECT_EQ(TERM, deposit.term);
  EXPECT_EQ(AMOUNT, deposit.amount);
  EXPECT_EQ(m_currency.calculateInterest(AMOUNT, TERM), deposit.interest);
  EXPECT_FALSE(deposit.locked);

  bob->shutdown();
}

TEST_F(WalletLegacyApi, serializeSpentDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();
  const uint64_t FEE2 = m_currency.minimumFee() + 10;

  auto id = makeDepositAndUnlock(AMOUNT, TERM, FEE);
  withdrawDeposits({id}, FEE2);

  std::stringstream data;
  alice->save(data);
  WaitWalletSave(aliceWalletObserver.get());

  alice->shutdown();

  prepareBobWallet();
  bob->initAndLoad(data, "pass");
  WaitWalletSync(bobWalletObserver.get());

  ASSERT_EQ(1, bob->getDepositCount());

  CryptoNote::Deposit deposit;
  EXPECT_TRUE(bob->getDeposit(0, deposit));
  EXPECT_EQ(1, deposit.creatingTransactionId);
  EXPECT_EQ(2, deposit.spendingTransactionId);
  EXPECT_EQ(TERM, deposit.term);
  EXPECT_EQ(AMOUNT, deposit.amount);
  EXPECT_EQ(m_currency.calculateInterest(AMOUNT, TERM), deposit.interest);
  EXPECT_FALSE(deposit.locked);

  bob->shutdown();
}

TEST_F(WalletLegacyApi, depositsUnlockAfterLoad) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  makeDeposit(AMOUNT, TERM, FEE);

  std::stringstream data;
  alice->save(data);
  WaitWalletSave(aliceWalletObserver.get());

  alice->shutdown();

  prepareBobWallet();
  bob->initAndLoad(data, "pass");
  WaitWalletSync(bobWalletObserver.get());

  generator.generateEmptyBlocks(TERM);
  bobNode->updateObservers();
  WaitWalletSync(bobWalletObserver.get());

  ASSERT_EQ(1, bob->getDepositCount());

  CryptoNote::Deposit deposit;
  EXPECT_TRUE(bob->getDeposit(0, deposit));
  EXPECT_FALSE(deposit.locked);

  bob->shutdown();
}

TEST_F(WalletLegacyApi, PaymentIdIndexWorks) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  for (int i = 0; i < 5; ++i) {
    GetOneBlockReward(*alice);
  }

  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  uint64_t sendAmount = 100000;
  
  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = sendAmount;

  std::vector<uint8_t> rawExtra;
  std::string extra;
  CryptoNote::PaymentId paymentId;
  ASSERT_TRUE(CryptoNote::createTxExtraWithPaymentId("deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef", rawExtra));
  std::copy(rawExtra.begin(), rawExtra.end(), std::back_inserter(extra));
  ASSERT_FALSE(extra.empty());
  ASSERT_TRUE(CryptoNote::getPaymentIdFromTxExtra(rawExtra, paymentId));

  ASSERT_EQ(0, bob->getTransactionCount());
  ASSERT_EQ(0, bob->getTransactionsByPaymentIds({paymentId})[0].transactions.size());

  aliceNode->setNextTransactionToPool();
  auto txId = alice->sendTransaction(tr, m_currency.minimumFee(), extra, 1, 0); 
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  ASSERT_EQ(1, bob->getTransactionCount());
  ASSERT_EQ(0, bob->getTransactionsByPaymentIds({ paymentId })[0].transactions.size());

  aliceNode->includeTransactionsFromPoolToBlock();

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  {
    auto payments = bob->getTransactionsByPaymentIds({paymentId});
    ASSERT_EQ(1, payments[0].transactions.size());
    ASSERT_EQ(sendAmount, payments[0].transactions[0].totalAmount);
  }

  {
    auto payments = alice->getTransactionsByPaymentIds({paymentId});
    ASSERT_EQ(0, payments[0].transactions.size());
  }
}
