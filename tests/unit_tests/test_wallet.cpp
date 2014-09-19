// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
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

#include <future>
#include <chrono>
#include <array>

#include "INode.h"
#include "wallet/Wallet.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/Currency.h"

#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"

class TrivialWalletObserver : public CryptoNote::IWalletObserver
{
public:
  TrivialWalletObserver() {}

  bool waitForSyncEnd() {
    auto future = syncPromise.get_future();
    return future.wait_for(std::chrono::seconds(3)) == std::future_status::ready;
  }

  bool waitForSendEnd(std::error_code& ec) {
    auto future = sendPromise.get_future();

    if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
      return false;
    }

    ec = future.get();
    return true;

  }

  bool waitForSaveEnd(std::error_code& ec) {
    auto future = savePromise.get_future();

    if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
      return false;
    }

    ec = future.get();
    return true;
  }

  bool waitForLoadEnd(std::error_code& ec) {
    auto future = loadPromise.get_future();

    if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
      return false;
    }

    ec = future.get();
    return true;
  }

  void reset() {
    syncPromise = std::promise<void>();
    sendPromise = std::promise<std::error_code>();
    savePromise = std::promise<std::error_code>();
    loadPromise = std::promise<std::error_code>();
  }

  virtual void synchronizationProgressUpdated(uint64_t current, uint64_t total, std::error_code result) {
    if (result) {
      syncPromise.set_value();
      return;
    }

    if (current == total) {
      syncPromise.set_value();
    }
  }

  virtual void sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) {
    sendPromise.set_value(result);
  }

  virtual void saveCompleted(std::error_code result) {
    savePromise.set_value(result);
  }

  virtual void initCompleted(std::error_code result) {
    loadPromise.set_value(result);
  }

  virtual void actualBalanceUpdated(uint64_t actualBalance) {
//    std::cout << "actual balance: " << actualBalance << std::endl;
  }
  virtual void pendingBalanceUpdated(uint64_t pendingBalance) {
//    std::cout << "pending balance: " << pendingBalance << std::endl;
  }

  std::promise<void> syncPromise;
  std::promise<std::error_code> sendPromise;
  std::promise<std::error_code> savePromise;
  std::promise<std::error_code> loadPromise;
};

struct SaveOnInitWalletObserver: public CryptoNote::IWalletObserver {
  SaveOnInitWalletObserver(CryptoNote::Wallet* wallet) : wallet(wallet) {};
  virtual ~SaveOnInitWalletObserver() {}

  virtual void initCompleted(std::error_code result) {
    wallet->save(stream, true, true);
  }

  CryptoNote::Wallet* wallet;
  std::stringstream stream;
};

static const uint64_t TEST_BLOCK_REWARD = cryptonote::START_BLOCK_REWARD;

CryptoNote::TransactionId TransferMoney(CryptoNote::Wallet& from, CryptoNote::Wallet& to, int64_t amount, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "") {
  CryptoNote::Transfer transfer;
  transfer.amount = amount;
  transfer.address = to.getAddress();

  return from.sendTransaction(transfer, fee, extra, mixIn);
}

void WaitWalletSync(TrivialWalletObserver* observer) {
  observer->reset();
  ASSERT_TRUE(observer->waitForSyncEnd());
}

void WaitWalletSend(TrivialWalletObserver* observer) {
  std::error_code ec;
  observer->reset();
  ASSERT_TRUE(observer->waitForSendEnd(ec));
}

void WaitWalletSend(TrivialWalletObserver* observer, std::error_code& ec) {
  observer->reset();
  ASSERT_TRUE(observer->waitForSendEnd(ec));
}

void WaitWalletSave(TrivialWalletObserver* observer) {
  observer->reset();
  std::error_code ec;

  ASSERT_TRUE(observer->waitForSaveEnd(ec));
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
  aliceNode.reset(new INodeTrivialRefreshStub(generator));
  aliceWalletObserver.reset(new TrivialWalletObserver());

  alice.reset(new CryptoNote::Wallet(m_currency, *aliceNode));
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
  prepareCarolWallet();

  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  //unblock Alice's money
  generator.generateEmptyBlocks(10);
  uint64_t expectedBalance = TEST_BLOCK_REWARD;

  alice->startRefresh();

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->pendingBalance(), expectedBalance);
  EXPECT_EQ(alice->actualBalance(), expectedBalance);

  bob->initAndGenerate("pass2");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, transferAmount, fee, 0, ""));

  generator.generateEmptyBlocks(10);

  alice->startRefresh();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  bob->startRefresh();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  EXPECT_EQ(bob->pendingBalance(), transferAmount);
  EXPECT_EQ(bob->actualBalance(), transferAmount);

  EXPECT_EQ(alice->pendingBalance(), expectedBalance - transferAmount - fee);
  EXPECT_EQ(alice->actualBalance(), expectedBalance - transferAmount - fee);

  alice->shutdown();
  bob->shutdown();
}

void WaitWalletLoad(TrivialWalletObserver* observer, std::error_code& ec) {
  observer->reset();

  ASSERT_TRUE(observer->waitForLoadEnd(ec));
}

TEST_F(WalletApi, initAndSave) {
  SaveOnInitWalletObserver saveOnInit(alice.get());
  alice->addObserver(&saveOnInit);
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
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

  alice->startRefresh();

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
  alice->startRefresh();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  bob->initAndGenerate("pass2");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t fee = 100000;
  int64_t amount1 = 1230000;
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, amount1, fee, 0));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  int64_t amount2 = 1234500;
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, amount2, fee, 0));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

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
  EXPECT_EQ(tx.totalAmount, amount1 + fee);
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 0);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransaction(2, tx));
  EXPECT_EQ(tx.totalAmount, amount2 + fee);
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 1);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransaction(3, tx));
  EXPECT_EQ(tx.totalAmount, amount3 + fee);
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 2);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransaction(4, tx));
  EXPECT_EQ(tx.totalAmount, amount4 + fee);
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
  alice->startRefresh();
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

  alice->shutdown();

  prepareAliceWallet();

  alice->initAndLoad(archive, "pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(alice->getTransactionCount(), 3);
  ASSERT_EQ(alice->getTransferCount(), 3);

  CryptoNote::TransactionInfo tx;
  ASSERT_TRUE(alice->getTransaction(1, tx));
  EXPECT_EQ(tx.totalAmount, amount1 + amount2 + fee);
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.firstTransferId, 0);
  EXPECT_EQ(tx.transferCount, 2);

  ASSERT_TRUE(alice->getTransaction(2, tx));
  EXPECT_EQ(tx.totalAmount, amount3 + fee);
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

  alice->startRefresh();
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
  alice->startRefresh();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  aliceNode->startAlternativeChain(3);
  generator.generateEmptyBlocks(10);
  alice->startRefresh();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->actualBalance(), 0);
  EXPECT_EQ(alice->pendingBalance(), 0);

  alice->shutdown();
}

TEST_F(WalletApi, saveAndLoadErroneousTxsCacheDetails) {
  prepareBobWallet();
  prepareCarolWallet();

  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));
  generator.generateEmptyBlocks(10);
  alice->startRefresh();
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

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);

  EXPECT_EQ(alice->getTransactionCount(), 2);
  EXPECT_EQ(alice->getTransferCount(), 2);

  CryptoNote::TransactionInfo tx;
  ASSERT_TRUE(alice->getTransaction(1, tx));
  EXPECT_EQ(tx.totalAmount, amounts[3] + amounts[4] + fee);
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
  alice->startRefresh();
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

  EXPECT_EQ(alice->getTransactionCount(), 2);
  EXPECT_EQ(alice->getTransferCount(), 0);

  CryptoNote::TransactionInfo tx;
  ASSERT_TRUE(alice->getTransaction(1, tx));
  EXPECT_EQ(tx.totalAmount, amounts[3] + amounts[4] + fee);
  EXPECT_EQ(tx.firstTransferId, CryptoNote::INVALID_TRANSFER_ID);
  EXPECT_EQ(tx.transferCount, 0);

  alice->shutdown();
}
