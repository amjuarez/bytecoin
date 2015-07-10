// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include <future>
#include <chrono>
#include <array>

#include "EventWaiter.h"
#include "INode.h"
#include "wallet/Wallet.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/Currency.h"

#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"

namespace {

class TrivialWalletObserver : public CryptoNote::IWalletObserver
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

struct SaveOnInitWalletObserver: public CryptoNote::IWalletObserver {
  SaveOnInitWalletObserver(CryptoNote::Wallet* wallet) : wallet(wallet) {};
  virtual ~SaveOnInitWalletObserver() {}

  virtual void initCompleted(std::error_code result) override {
    wallet->save(stream, true, true);
  }

  CryptoNote::Wallet* wallet;
  std::stringstream stream;
};

static const uint64_t TEST_BLOCK_REWARD = cryptonote::START_BLOCK_REWARD;

CryptoNote::TransactionId TransferMoney(CryptoNote::Wallet& from, CryptoNote::Wallet& to, int64_t amount, uint64_t fee,
    uint64_t mixIn = 0, const std::string& extra = "", const std::vector<CryptoNote::TransactionMessage>& messages = std::vector<CryptoNote::TransactionMessage>()) {
  CryptoNote::Transfer transfer;
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

class ScopedObserverBase : public CryptoNote::IWalletObserver {
public:
  ScopedObserverBase(CryptoNote::IWallet& wallet) : m_wallet(wallet) {
    m_wallet.addObserver(this);
  }

  ScopedObserverBase(const ScopedObserverBase&) = delete;
  ScopedObserverBase(ScopedObserverBase&&) = delete;

  virtual ~ScopedObserverBase() {
    m_wallet.removeObserver(this);
  }

protected:
  CryptoNote::IWallet& m_wallet;
  EventWaiter called;
};

class DepositsUpdatedScopedObserver : public ScopedObserverBase {
public:
  DepositsUpdatedScopedObserver(CryptoNote::IWallet& wallet) : ScopedObserverBase(wallet) {}
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
  DepositsActualBalanceChangedScopedObserver(CryptoNote::IWallet& wallet) : ScopedObserverBase(wallet) {}
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
  DepositsPendingBalanceChangedScopedObserver(CryptoNote::IWallet& wallet) : ScopedObserverBase(wallet) {}
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
  PendingBalanceChangedScopedObserver(CryptoNote::IWallet& wallet) : ScopedObserverBase(wallet) {}
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
  ActualBalanceChangedScopedObserver(CryptoNote::IWallet& wallet) : ScopedObserverBase(wallet) {}
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

} //namespace

class WalletApi : public ::testing::Test
{
public:
  WalletApi() : m_currency(cryptonote::CurrencyBuilder().depositMinTerm(100).depositMinTotalRateFactor(0).defaultDustThreshold(0).currency()), generator(m_currency) {
  }

  void SetUp();

protected:
  void prepareAliceWallet();
  void prepareBobWallet();
  void prepareCarolWallet();

  void GetOneBlockReward(CryptoNote::Wallet& wallet);
  void GenerateOneBlockRewardAndUnlock();

  void TestSendMoney(int64_t transferAmount, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "");
  void performTransferWithErrorTx(const std::array<int64_t, 5>& amounts, uint64_t fee);

  CryptoNote::DepositId makeDeposit(uint64_t amount, uint64_t term, uint64_t fee, uint64_t mixin = 0);
  void unlockDeposit(uint64_t term);
  CryptoNote::DepositId makeDepositAndUnlock(uint64_t amount, uint64_t term, uint64_t fee, uint64_t mixin = 0);
  CryptoNote::TransactionId withdrawDeposits(const std::vector<CryptoNote::DepositId>& ids, uint64_t fee);
  uint64_t calculateTotalDepositAmount(uint64_t amount, uint64_t term);

  cryptonote::Currency m_currency;

  TestBlockchainGenerator generator;

  std::shared_ptr<TrivialWalletObserver> aliceWalletObserver;
  std::shared_ptr<INodeTrivialRefreshStub> aliceNode;
  std::shared_ptr<CryptoNote::Wallet> alice;

  std::shared_ptr<TrivialWalletObserver> bobWalletObserver;
  std::shared_ptr<INodeTrivialRefreshStub> bobNode;
  std::shared_ptr<CryptoNote::Wallet> bob;

  std::shared_ptr<TrivialWalletObserver> carolWalletObserver;
  std::shared_ptr<INodeTrivialRefreshStub> carolNode;
  std::shared_ptr<CryptoNote::Wallet> carol;
};

void WalletApi::SetUp() {
  prepareAliceWallet();
  generator.generateEmptyBlocks(3);
}

void WalletApi::prepareAliceWallet() {
  decltype(aliceNode) newNode(new INodeTrivialRefreshStub(generator));

  alice.reset(new CryptoNote::Wallet(m_currency, *newNode));
  aliceNode = newNode;

  aliceWalletObserver.reset(new TrivialWalletObserver());
  alice->addObserver(aliceWalletObserver.get());
}

void WalletApi::prepareBobWallet() {
  bobNode.reset(new INodeTrivialRefreshStub(generator));
  bobWalletObserver.reset(new TrivialWalletObserver());

  bob.reset(new CryptoNote::Wallet(m_currency, *bobNode));
  bob->addObserver(bobWalletObserver.get());
}

void WalletApi::prepareCarolWallet() {
  carolNode.reset(new INodeTrivialRefreshStub(generator));
  carolWalletObserver.reset(new TrivialWalletObserver());

  carol.reset(new CryptoNote::Wallet(m_currency, *carolNode));
  carol->addObserver(carolWalletObserver.get());
}

void WalletApi::GetOneBlockReward(CryptoNote::Wallet& wallet) {
  cryptonote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(wallet.getAddress(), address));
  generator.getBlockRewardForAddress(address);
}

//may be called only after prepareAliceWallet and alice->initAndGenerate
void WalletApi::GenerateOneBlockRewardAndUnlock() {
  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
}

void WalletApi::performTransferWithErrorTx(const std::array<int64_t, 5>& amounts, uint64_t fee) {
  std::vector<CryptoNote::Transfer> trs;
  CryptoNote::Transfer tr;
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

void WalletApi::TestSendMoney(int64_t transferAmount, uint64_t fee, uint64_t mixIn, const std::string& extra) {
  prepareBobWallet();

  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  //unblock Alice's money
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

  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, transferAmount, fee, 0, ""));

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

CryptoNote::DepositId WalletApi::makeDeposit(uint64_t amount, uint64_t term, uint64_t fee, uint64_t mixin) {
  auto txId = alice->deposit(term, amount, fee, mixin);
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());

  CryptoNote::TransactionInfo txInfo;
  alice->getTransaction(txId, txInfo);

  return txInfo.firstDepositId;
}

void WalletApi::unlockDeposit(uint64_t term) {
  generator.generateEmptyBlocks(term - 1); //subtract 1 becaause INodeTrivialRefreshStub->relayTransaction adds new block implicitly
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());
}

CryptoNote::DepositId WalletApi::makeDepositAndUnlock(uint64_t amount, uint64_t term, uint64_t fee, uint64_t mixin) {
  auto id = makeDeposit(amount, term, fee, mixin);
  unlockDeposit(term);

  return id;
}

CryptoNote::TransactionId WalletApi::withdrawDeposits(const std::vector<CryptoNote::DepositId>& ids, uint64_t fee) {
  auto txId = alice->withdrawDeposits(ids, fee);
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());

  return txId;
}

uint64_t WalletApi::calculateTotalDepositAmount(uint64_t amount, uint64_t term) {
  return m_currency.calculateInterest(amount, term) + amount;
}

TEST_F(WalletApi, initAndSave) {
  SaveOnInitWalletObserver saveOnInit(alice.get());
  alice->addObserver(&saveOnInit);
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));
  alice->shutdown();
}

TEST_F(WalletApi, refreshWithMoney) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(alice->actualBalance(), 0);
  ASSERT_EQ(alice->pendingBalance(), 0);

  cryptonote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getBlockRewardForAddress(address);

  aliceNode->updateObservers();

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->actualBalance(), 0);
  EXPECT_EQ(alice->pendingBalance(), TEST_BLOCK_REWARD);

  alice->shutdown();
}

TEST_F(WalletApi, initWithMoney) {
  std::stringstream archive;

  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  alice->save(archive, true, true);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  ASSERT_EQ(alice->actualBalance(), 0);
  ASSERT_EQ(alice->pendingBalance(), 0);

  cryptonote::AccountPublicAddress address;
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

TEST_F(WalletApi, TransactionsAndTransfersAfterSend) {
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

  CryptoNote::TransactionInfo tx;

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
  CryptoNote::Transfer tr;
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

TEST_F(WalletApi, saveAndLoadCacheDetails) {
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

  std::vector<CryptoNote::Transfer> trs;
  CryptoNote::Transfer tr;
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

  CryptoNote::TransactionInfo tx;
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

TEST_F(WalletApi, sendMoneySuccessNoMixin) {
  ASSERT_NO_FATAL_FAILURE(TestSendMoney(10000000, 1000000, 0));
}

TEST_F(WalletApi, sendMoneySuccessWithMixin) {
  ASSERT_NO_FATAL_FAILURE(TestSendMoney(10000000, 1000000, 3));
}

TEST_F(WalletApi, getTransactionSuccess) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::TransactionInfo tx;

  ASSERT_EQ(alice->getTransactionCount(), 1);
  ASSERT_TRUE(alice->getTransaction(0, tx));

  EXPECT_EQ(tx.firstTransferId, CryptoNote::INVALID_TRANSFER_ID);
  EXPECT_EQ(tx.transferCount, 0);
  EXPECT_EQ(tx.totalAmount, TEST_BLOCK_REWARD);
  EXPECT_EQ(tx.fee, 0);
  EXPECT_EQ(tx.isCoinbase, false);

  alice->shutdown();
}

TEST_F(WalletApi, getTransactionFailure) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::TransactionInfo tx;

  ASSERT_EQ(alice->getTransactionCount(), 0);
  ASSERT_FALSE(alice->getTransaction(0, tx));

  alice->shutdown();
}

TEST_F(WalletApi, useNotInitializedObject) {
  EXPECT_THROW(alice->pendingBalance(), std::system_error);
  EXPECT_THROW(alice->actualBalance(), std::system_error);
  EXPECT_THROW(alice->getTransactionCount(), std::system_error);
  EXPECT_THROW(alice->getTransferCount(), std::system_error);
  EXPECT_THROW(alice->getAddress(), std::system_error);

  std::stringstream archive;
  EXPECT_THROW(alice->save(archive, true, true), std::system_error);

  EXPECT_THROW(alice->findTransactionByTransferId(1), std::system_error);

  CryptoNote::TransactionInfo tx;
  CryptoNote::Transfer tr;
  EXPECT_THROW(alice->getTransaction(1, tx), std::system_error);
  EXPECT_THROW(alice->getTransfer(2, tr), std::system_error);

  tr.address = "lslslslslslsls";
  tr.amount = 1000000;
  EXPECT_THROW(alice->sendTransaction(tr, 300201), std::system_error);

  std::vector<CryptoNote::Transfer> trs;
  trs.push_back(tr);
  EXPECT_THROW(alice->sendTransaction(trs, 329293), std::system_error);
}

TEST_F(WalletApi, sendWrongAmount) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::Transfer tr;
  tr.address = "1234567890qwertasdfgzxcvbyuiophjklnm";
  tr.amount = 1;

  EXPECT_THROW(alice->sendTransaction(tr, 1), std::system_error);

  alice->shutdown();
}

TEST_F(WalletApi, wrongPassword) {
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
  EXPECT_EQ(result.value(), cryptonote::error::WRONG_PASSWORD);
}

TEST_F(WalletApi, detachBlockchain) {
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

TEST_F(WalletApi, saveAndLoad) {
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

TEST_F(WalletApi, DISABLED_saveAndLoadErroneousTxsCacheDetails) {
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

  CryptoNote::TransactionInfo tx;
  ASSERT_TRUE(alice->getTransaction(1, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amounts[3] + amounts[4] + fee));
  EXPECT_EQ(tx.firstTransferId, 0);
  EXPECT_EQ(tx.transferCount, 2);

  CryptoNote::Transfer tr;
  ASSERT_TRUE(alice->getTransfer(0, tr));
  EXPECT_EQ(tr.amount, amounts[3]);
  EXPECT_EQ(tr.address, bob->getAddress());

  ASSERT_TRUE(alice->getTransfer(1, tr));
  EXPECT_EQ(tr.amount, amounts[4]);
  EXPECT_EQ(tr.address, carol->getAddress());

  alice->shutdown();
}

TEST_F(WalletApi, DISABLED_saveAndLoadErroneousTxsCacheNoDetails) {
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

TEST_F(WalletApi, mineSaveNoCacheNoDetailsRefresh) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  cryptonote::AccountPublicAddress address;
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


TEST_F(WalletApi, sendMoneyToMyself) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  cryptonote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getBlockRewardForAddress(address);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::TransactionId txId = TransferMoney(*alice, *alice, 100000000, 100);
  ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD - 100, alice->actualBalance());
  ASSERT_EQ(0, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletApi, sendSeveralTransactions) {
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
    CryptoNote::Transfer tr;
    tr.address = bob->getAddress();
    tr.amount = sendAmount;

    auto txId = alice->sendTransaction(tr, m_currency.minimumFee(), "", 1, 0);  
    ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);

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

TEST_F(WalletApi, balanceAfterFailedTransaction) {
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

  CryptoNote::Transfer tr;
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

TEST_F(WalletApi, checkPendingBalance) {
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

  CryptoNote::Transfer tr;
  tr.address = bob->getAddress();
  tr.amount = sendAmount;

  auto txId = alice->sendTransaction(tr, fee, "", 1, 0);
  ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);

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

TEST_F(WalletApi, checkChange) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t banknote = 1000000000;
  uint64_t sendAmount = 50000;
  uint64_t fee = m_currency.minimumFee();

  cryptonote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::Transfer tr;
  tr.address = bob->getAddress();
  tr.amount = sendAmount;

  auto txId = alice->sendTransaction(tr, fee, "", 1, 0);
  ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);

  std::error_code sendResult;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), sendResult));
  ASSERT_EQ(std::error_code(), sendResult);

  EXPECT_EQ(0, alice->actualBalance());
  EXPECT_EQ(banknote - sendAmount - fee, alice->pendingBalance());
}

TEST_F(WalletApi, checkBalanceAfterSend) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  uint64_t banknote = 1000000000;

  cryptonote::AccountPublicAddress address;
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
  ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  ASSERT_EQ(banknote, alice->actualBalance());
  ASSERT_EQ(banknote - sendAmount - fee, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletApi, moneyInPoolDontAffectActualBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t banknote = 1000000000;

  cryptonote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  const uint64_t sendAmount = 10000000;
  const uint64_t fee = 100;
  aliceNode->setNextTransactionToPool();
  CryptoNote::TransactionId txId = TransferMoney(*alice, *bob, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(0, alice->actualBalance());
  EXPECT_EQ(banknote - sendAmount - fee, alice->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletApi, balanceAfterTransactionsPlacedInBlockchain) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t banknote = 1000000000;

  cryptonote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  const uint64_t sendAmount = 10000000;
  const uint64_t fee = 100;
  aliceNode->setNextTransactionToPool();
  CryptoNote::TransactionId txId = TransferMoney(*alice, *bob, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);
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

TEST_F(WalletApi, checkMyMoneyInTxPool) {
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
  ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  EXPECT_EQ(0, bob->actualBalance());
  EXPECT_EQ(sendAmount, bob->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletApi, initWithKeys) {
  CryptoNote::WalletAccountKeys accountKeys;

  uint8_t byte = 0;

  std::generate(accountKeys.spendPublicKey.begin(), accountKeys.spendPublicKey.end(),
    [&byte] () { return byte++; } );

  std::generate(accountKeys.spendSecretKey.begin(), accountKeys.spendSecretKey.end(),
    [&byte] () { return byte++; } );

  std::generate(accountKeys.viewPublicKey.begin(), accountKeys.viewPublicKey.end(),
    [&byte] () { return byte++; } );

  std::generate(accountKeys.viewSecretKey.begin(), accountKeys.viewSecretKey.end(),
    [&byte] () { return byte++; } );

  alice->initWithKeys(accountKeys, "pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get()));

  CryptoNote::WalletAccountKeys keys;
  alice->getAccountKeys(keys);

  EXPECT_TRUE(std::equal(accountKeys.spendPublicKey.begin(), accountKeys.spendPublicKey.end(), keys.spendPublicKey.begin()));
  EXPECT_TRUE(std::equal(accountKeys.spendSecretKey.begin(), accountKeys.spendSecretKey.end(), keys.spendSecretKey.begin()));
  EXPECT_TRUE(std::equal(accountKeys.viewPublicKey.begin(), accountKeys.viewPublicKey.end(), keys.viewPublicKey.begin()));
  EXPECT_TRUE(std::equal(accountKeys.viewSecretKey.begin(), accountKeys.viewSecretKey.end(), keys.viewSecretKey.begin()));

  alice->shutdown();
}

TEST_F(WalletApi, deleteTxFromPool) {
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
  ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);
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

TEST_F(WalletApi, sendAfterFailedTransaction) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::Transfer tr;
  tr.amount = 100000;
  tr.address = "wrong_address";

  EXPECT_THROW(alice->sendTransaction(tr, 1000, "", 2, 0), std::system_error);
  CryptoNote::TransactionId txId = TransferMoney(*alice, *alice, 100000, 100);
  ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  alice->shutdown();
}

TEST_F(WalletApi, loadingBrokenCache) {
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

TEST_F(WalletApi, sendMessage) {
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
  CryptoNote::TransactionInfo tx;
  ASSERT_TRUE(bob->getTransaction(0, tx));
  ASSERT_EQ(1, tx.messages.size());
  ASSERT_EQ(text, tx.messages[0]);

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletApi, sendBulkOfMessages) {
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

  std::vector<CryptoNote::Transfer> transfers;
  transfers.push_back({ bob->getAddress(), 100 });
  transfers.push_back({ carol->getAddress(), 100 });

  alice->sendTransaction(transfers, 10, std::string(), 0, 0, messages);

  generator.generateEmptyBlocks(1);
  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  carolNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  CryptoNote::TransactionInfo bobTx;
  ASSERT_EQ(1, bob->getTransactionCount());
  ASSERT_TRUE(bob->getTransaction(0, bobTx));
  ASSERT_EQ(4, bobTx.messages.size());
  //there's no guarantee of any particular order
  ASSERT_NE(bobTx.messages.end(), std::find(bobTx.messages.begin(), bobTx.messages.end(), verse1));
  ASSERT_NE(bobTx.messages.end(), std::find(bobTx.messages.begin(), bobTx.messages.end(), chorus));
  ASSERT_NE(bobTx.messages.end(), std::find(bobTx.messages.begin(), bobTx.messages.end(), verse2));
  ASSERT_NE(bobTx.messages.end(), std::find(bobTx.messages.begin(), bobTx.messages.end(), verse3));

  CryptoNote::TransactionInfo carolTx;
  ASSERT_EQ(1, carol->getTransactionCount());
  ASSERT_TRUE(carol->getTransaction(0, carolTx));
  ASSERT_EQ(0, carolTx.messages.size());

  alice->shutdown();
  bob->shutdown();
  carol->shutdown();
}

TEST_F(WalletApi, depositReturnsCorrectDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();
  const uint64_t AMOUNT = m_currency.depositMinAmount();

  auto txId = alice->deposit(TERM, AMOUNT, FEE);
  WaitWalletSend(aliceWalletObserver.get());

  CryptoNote::TransactionInfo info;
  ASSERT_TRUE(alice->getTransaction(txId, info));

  EXPECT_EQ(0, info.firstDepositId);
  EXPECT_EQ(1, info.depositCount);
  EXPECT_EQ(-static_cast<int64_t>(AMOUNT + FEE), info.totalAmount);
  EXPECT_EQ(CryptoNote::INVALID_TRANSFER_ID, info.firstTransferId);
  EXPECT_EQ(0, info.transferCount);
  EXPECT_EQ(FEE, info.fee);

  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(0, deposit));
  EXPECT_EQ(txId, deposit.creatingTransactionId);
  EXPECT_EQ(CryptoNote::INVALID_TRANSACTION_ID, deposit.spendingTransactionId);
  EXPECT_EQ(TERM, deposit.term);
  EXPECT_EQ(AMOUNT, deposit.amount);
  EXPECT_EQ(m_currency.calculateInterest(deposit.amount, deposit.term), deposit.interest);

  alice->shutdown();
}

TEST_F(WalletApi, depositWithMixinReturnsCorrectDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint32_t TERM = m_currency.depositMinTerm();
  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t FEE = m_currency.minimumFee();

  auto txId = alice->deposit(TERM, AMOUNT, FEE, 3);
  WaitWalletSend(aliceWalletObserver.get());

  CryptoNote::TransactionInfo info;
  ASSERT_TRUE(alice->getTransaction(txId, info));

  EXPECT_EQ(0, info.firstDepositId);
  EXPECT_EQ(1, info.depositCount);
  EXPECT_EQ(-static_cast<int64_t>(AMOUNT + FEE), info.totalAmount);
  EXPECT_EQ(CryptoNote::INVALID_TRANSFER_ID, info.firstTransferId);
  EXPECT_EQ(0, info.transferCount);
  EXPECT_EQ(FEE, info.fee);

  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(0, deposit));
  EXPECT_EQ(txId, deposit.creatingTransactionId);
  EXPECT_EQ(CryptoNote::INVALID_TRANSACTION_ID, deposit.spendingTransactionId);
  EXPECT_EQ(TERM, deposit.term);
  EXPECT_EQ(AMOUNT, deposit.amount);
  EXPECT_EQ(m_currency.calculateInterest(deposit.amount, deposit.term), deposit.interest);

  alice->shutdown();
}

TEST_F(WalletApi, depositsUpdatedCallbackCame) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  alice->deposit(m_currency.depositMinTerm(), m_currency.depositMinAmount(), m_currency.minimumFee(), 3);
  ASSERT_TRUE(aliceWalletObserver->waitForDepositsUpdated());

  alice->shutdown();
}

TEST_F(WalletApi, depositsRestoredAfterSerialization) {
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
  EXPECT_EQ(CryptoNote::INVALID_TRANSACTION_ID, deposit1.spendingTransactionId);
  EXPECT_EQ(m_currency.calculateInterest(deposit1.amount, deposit1.term), deposit1.interest);

  CryptoNote::Deposit deposit2;
  ASSERT_TRUE(bob->getDeposit(1, deposit2));
  EXPECT_EQ(AMOUNT2, deposit2.amount);
  EXPECT_EQ(TERM2, deposit2.term);
  EXPECT_EQ(secondTx, deposit2.creatingTransactionId);
  EXPECT_EQ(CryptoNote::INVALID_TRANSACTION_ID, deposit2.spendingTransactionId);
  EXPECT_EQ(m_currency.calculateInterest(deposit2.amount, deposit2.term), deposit2.interest);

  bob->shutdown();
}

TEST_F(WalletApi, depositsRestoredFromBlockchain) {
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
  EXPECT_EQ(CryptoNote::INVALID_TRANSACTION_ID, unlockedDeposit.spendingTransactionId);
  EXPECT_FALSE(unlockedDeposit.locked);

  CryptoNote::Deposit lockedDeposit;
  bob->getDeposit(lockedDepositId, lockedDeposit);
  EXPECT_EQ(AMOUNT2, lockedDeposit.amount);
  EXPECT_EQ(TERM, lockedDeposit.term);
  EXPECT_EQ(m_currency.calculateInterest(AMOUNT2, TERM), lockedDeposit.interest);
  EXPECT_EQ(lockedDepositCreatingTransactionId, lockedDeposit.creatingTransactionId);
  EXPECT_EQ(CryptoNote::INVALID_TRANSACTION_ID, lockedDeposit.spendingTransactionId);
  EXPECT_TRUE(lockedDeposit.locked);

  bob->shutdown();
}

TEST_F(WalletApi, depositsUnlock) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto walletActualBalance = alice->actualBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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

TEST_F(WalletApi, depositsWithTooSmallTerm) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm() - 1;
  const uint64_t FEE = m_currency.minimumFee();

  ASSERT_ANY_THROW(makeDeposit(AMOUNT, TERM, FEE));
  alice->shutdown();
}

TEST_F(WalletApi, depositsWithTooBigTerm) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMaxTerm() + 1;
  const uint64_t FEE = m_currency.minimumFee();

  ASSERT_ANY_THROW(makeDeposit(AMOUNT, TERM, FEE));
  alice->shutdown();
}

TEST_F(WalletApi, depositsWithTooSmallAmount) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount() - 1;
  const uint64_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  ASSERT_ANY_THROW(makeDeposit(AMOUNT, TERM, FEE));
  alice->shutdown();
}

TEST_F(WalletApi, depositsUpdatedCallbackCalledOnDepositUnlock) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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

TEST_F(WalletApi, depositsWithdraw) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();
  const uint64_t FEE2 = m_currency.minimumFee();

  auto id = makeDepositAndUnlock(AMOUNT, TERM, FEE);

  withdrawDeposits({id}, FEE2);
  EXPECT_EQ(calculateTotalDepositAmount(AMOUNT, TERM) - FEE2, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletApi, depositsCheckSpendingTransactionId) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto id = makeDepositAndUnlock(AMOUNT, TERM, FEE);
  auto spendingTxId = withdrawDeposits({id}, FEE);

  CryptoNote::Deposit deposit;
  ASSERT_TRUE(alice->getDeposit(id, deposit));
  EXPECT_EQ(spendingTxId, deposit.spendingTransactionId);

  alice->shutdown();
}

TEST_F(WalletApi, depositsWithdrawTwoDepositsCheckSpendingTransactionId) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t AMOUNT2 = m_currency.depositMinAmount() + 1;
  const uint64_t TERM = m_currency.depositMinTerm();
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

TEST_F(WalletApi, depositsWithdrawWrongDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  ASSERT_ANY_THROW(withdrawDeposits({3}, m_currency.minimumFee()));

  alice->shutdown();
}

TEST_F(WalletApi, depositsWithdrawLockedDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto depositId = makeDeposit(AMOUNT, TERM, FEE);
  unlockDeposit(TERM - 1);

  ASSERT_ANY_THROW(withdrawDeposits({depositId}, FEE));

  alice->shutdown();
}

TEST_F(WalletApi, depositsWithdrawFeeGreaterThenAmount) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto depositId = makeDeposit(AMOUNT, TERM, FEE);
  unlockDeposit(TERM);

  ASSERT_ANY_THROW(withdrawDeposits({depositId}, calculateTotalDepositAmount(AMOUNT, TERM) + 1));

  alice->shutdown();
}

TEST_F(WalletApi, depositsUpdatedCallbackCalledOnWithdraw) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t AMOUNT2 = m_currency.depositMinAmount() + 1;
  const uint64_t TERM = m_currency.depositMinTerm();
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

TEST_F(WalletApi, depositsBalancesRightAfterMakingDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto initialActualBalance = alice->actualBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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

TEST_F(WalletApi, depositsBalancesAfterUnlockingDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto initialTotalBalance = alice->actualBalance() + alice->pendingBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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

TEST_F(WalletApi, depositsBalancesAfterWithdrawDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto initialActualBalance = alice->actualBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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

TEST_F(WalletApi, lockedDepositsRemovedAfterDetach) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto initialActualBalance = alice->actualBalance();
  auto initialPendingBalance = alice->pendingBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
  const uint64_t FEE = m_currency.minimumFee();

  auto detachHeight = generator.getCurrentHeight() - 1;

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

  CryptoNote::TransactionInfo txInfo;
  ASSERT_TRUE(alice->getTransaction(deposit.creatingTransactionId, txInfo));

  EXPECT_EQ(CryptoNote::TransactionState::Deleted, txInfo.state);

  alice->shutdown();
}

TEST_F(WalletApi, unlockedDepositsRemovedAfterDetach) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  auto initialActualBalance = alice->actualBalance();
  auto initialPendingBalance = alice->pendingBalance();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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

  CryptoNote::TransactionInfo txInfo;
  ASSERT_TRUE(alice->getTransaction(deposit.creatingTransactionId, txInfo));

  EXPECT_EQ(CryptoNote::TransactionState::Deleted, txInfo.state);

  alice->shutdown();
}

TEST_F(WalletApi, unlockedDepositsLockedAfterDetach) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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

TEST_F(WalletApi, serializeLockedDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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
  EXPECT_EQ(CryptoNote::INVALID_TRANSACTION_ID, deposit.spendingTransactionId);
  EXPECT_EQ(TERM, deposit.term);
  EXPECT_EQ(AMOUNT, deposit.amount);
  EXPECT_EQ(m_currency.calculateInterest(AMOUNT, TERM), deposit.interest);
  EXPECT_TRUE(deposit.locked);

  bob->shutdown();
}

TEST_F(WalletApi, serializeUnlockedDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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
  EXPECT_EQ(CryptoNote::INVALID_TRANSACTION_ID, deposit.spendingTransactionId);
  EXPECT_EQ(TERM, deposit.term);
  EXPECT_EQ(AMOUNT, deposit.amount);
  EXPECT_EQ(m_currency.calculateInterest(AMOUNT, TERM), deposit.interest);
  EXPECT_FALSE(deposit.locked);

  bob->shutdown();
}

TEST_F(WalletApi, serializeSpentDeposit) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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

TEST_F(WalletApi, depositsUnlockAfterLoad) {
  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  GenerateOneBlockRewardAndUnlock();

  const uint64_t AMOUNT = m_currency.depositMinAmount();
  const uint64_t TERM = m_currency.depositMinTerm();
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

TEST_F(WalletApi, PaymentIdIndexWorks) {
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
  
  CryptoNote::Transfer tr;
  tr.address = bob->getAddress();
  tr.amount = sendAmount;

  std::string extra;
  std::vector<uint8_t> rawExtra;
  ASSERT_TRUE(cryptonote::createTxExtraWithPaymentId("deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef", rawExtra));
  std::copy(rawExtra.begin(), rawExtra.end(), std::back_inserter(extra));
  CryptoNote::PaymentId paymentId;
  crypto::hash id;
  ASSERT_TRUE(cryptonote::getPaymentIdFromTxExtra(rawExtra, id));
  std::copy_n(reinterpret_cast<unsigned char*>(&id), sizeof(CryptoNote::PaymentId), std::begin(paymentId));
  ASSERT_EQ(0, bob->getTransactionCount());
  ASSERT_EQ(0, bob->getTransactionsByPaymentIds({paymentId})[0].transactions.size());
  aliceNode->setNextTransactionToPool();
  ASSERT_FALSE(extra.empty());
  auto txId = alice->sendTransaction(tr, m_currency.minimumFee(), extra, 1, 0); 

  bobNode->setNextTransactionToPool();
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  ASSERT_NE(txId, CryptoNote::INVALID_TRANSACTION_ID);
  aliceNode->includeTransactionsFromPoolToBlock();
  ASSERT_EQ(0, bob->getTransactionsByPaymentIds({paymentId})[0].transactions.size());

  bobNode->includeTransactionsFromPoolToBlock();
  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));
  ASSERT_EQ(1, bob->getTransactionCount());
  bobNode->includeTransactionsFromPoolToBlock();
  //ASSERT_EQ(0, bob->unconfirmedTransactionAmount());
  
  CryptoNote::TransactionInfo info;
  ASSERT_TRUE(bob->getTransaction(0, info));
  //CryptoNote::ITransfersObserver& obeserver = *bob;
  //observer.onTransactionUpdated(nullptr, info.hash);
  generator.generateEmptyBlocks(10);

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
