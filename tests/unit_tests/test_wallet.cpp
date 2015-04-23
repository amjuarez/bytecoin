// Copyright (c) 2011-2015 The Cryptonote developers
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

  std::error_code sendResult;
  std::error_code saveResult;
  std::error_code loadResult;

  std::atomic<uint64_t> actualBalance;
  std::atomic<uint64_t> pendingBalance;

  EventWaiter synced;
  EventWaiter saved;
  EventWaiter loaden;
  EventWaiter sent;
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

static const uint64_t TEST_BLOCK_REWARD = 70368744177663;

CryptoNote::TransactionId TransferMoney(CryptoNote::Wallet& from, CryptoNote::Wallet& to, int64_t amount, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "") {
  CryptoNote::Transfer transfer;
  transfer.amount = amount;
  transfer.address = to.getAddress();

  return from.sendTransaction(transfer, fee, extra, mixIn);
}

void WaitWalletSync(TrivialWalletObserver* observer) {
  ASSERT_TRUE(observer->waitForSyncEnd());
}

void WaitWalletSend(TrivialWalletObserver* observer) {
  std::error_code ec;
  ASSERT_TRUE(observer->waitForSendEnd(ec));
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

class WalletApi : public ::testing::Test
{
public:
  WalletApi() : m_currency(cryptonote::CurrencyBuilder().currency()), generator(m_currency) {
  }

  void SetUp();

protected:
  void prepareAliceWallet();
  void prepareBobWallet();
  void prepareCarolWallet();

  void GetOneBlockReward(CryptoNote::Wallet& wallet);

  void TestSendMoney(int64_t transferAmount, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "");
  void performTransferWithErrorTx(const std::array<int64_t, 5>& amounts, uint64_t fee);

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

TEST_F(WalletApi, saveAndLoadErroneousTxsCacheDetails) {
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

TEST_F(WalletApi, saveAndLoadErroneousTxsCacheNoDetails) {
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

  generator.generateEmptyBlocks(1);
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
