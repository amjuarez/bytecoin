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

#include "cryptonote_core/account.h"
#include "cryptonote_core/CoreConfig.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/Currency.h"
#include "Logging/LoggerManager.h"
#include "p2p/NetNodeConfig.h"
#include "System/Dispatcher.h"
#include "System/InterruptedException.h"
#include "wallet/Wallet.h"

#include "../integration_test_lib/BaseFunctionalTest.h"
#include "../integration_test_lib/TestWallet.h"


using namespace CryptoNote;

extern System::Dispatcher globalSystem;

namespace {
  TransactionHash toTransactionHash(const crypto::hash& h) {
    TransactionHash result;
    std::copy(reinterpret_cast<const uint8_t*>(&h), reinterpret_cast<const uint8_t*>(&h) + sizeof(h), result.begin());
    return result;
  }

  class NodeTxPoolSyncTest : public Tests::Common::BaseFunctionalTest, public ::testing::Test {
  public:
    NodeTxPoolSyncTest() :
        BaseFunctionalTest(m_currency, globalSystem, Tests::Common::BaseFunctionalTestConfig()),
        m_dispatcher(globalSystem),
        m_currency(CurrencyBuilder(m_logManager).testnet(true).currency()) {
    }

  protected:
    Logging::LoggerManager m_logManager;
    System::Dispatcher& m_dispatcher;
    CryptoNote::Currency m_currency;
  };

  const std::string TEST_PASSWORD = "password";

  class TestWallet : private IWalletObserver {
  public:
    TestWallet(System::Dispatcher& dispatcher, Currency& currency, INode& node) :
        m_dispatcher(dispatcher),
        m_synchronizationCompleted(dispatcher),
        m_someTransactionUpdated(dispatcher),
        m_currency(currency),
        m_node(node),
        m_wallet(new CryptoNote::Wallet(currency, node)),
        m_currentHeight(0) {
      m_wallet->addObserver(this);
    }

    ~TestWallet() {
      m_wallet->removeObserver(this);
    }

    std::error_code init() {
      CryptoNote::account_base walletAccount;
      walletAccount.generate();

      WalletAccountKeys walletKeys;
      walletKeys.spendPublicKey = reinterpret_cast<const WalletPublicKey&>(walletAccount.get_keys().m_account_address.m_spendPublicKey);
      walletKeys.spendSecretKey = reinterpret_cast<const WalletSecretKey&>(walletAccount.get_keys().m_spend_secret_key);
      walletKeys.viewPublicKey = reinterpret_cast<const WalletPublicKey&>(walletAccount.get_keys().m_account_address.m_viewPublicKey);
      walletKeys.viewSecretKey = reinterpret_cast<const WalletSecretKey&>(walletAccount.get_keys().m_view_secret_key);

      m_wallet->initWithKeys(walletKeys, TEST_PASSWORD);
      m_synchronizationCompleted.wait();
      return m_lastSynchronizationResult;
    }

    struct TransactionSendingWaiter : public IWalletObserver {
      System::Dispatcher& m_dispatcher;
      System::Event m_event;
      bool m_waiting = false;
      TransactionId m_expectedTxId;
      std::error_code m_result;

      TransactionSendingWaiter(System::Dispatcher& dispatcher) : m_dispatcher(dispatcher), m_event(dispatcher) {
      }

      void wait(TransactionId expectedTxId) {
        m_waiting = true;
        m_expectedTxId = expectedTxId;
        m_event.wait();
        m_waiting = false;
      }

      virtual void sendTransactionCompleted(TransactionId transactionId, std::error_code result) {
        m_dispatcher.remoteSpawn([this, transactionId, result]() {
          if (m_waiting &&  m_expectedTxId == transactionId) {
            m_result = result;
            m_event.set();
          }
        });
      }
    };

    std::error_code sendTransaction(const std::string& address, uint64_t amount, TransactionHash& txHash) {
      TransactionSendingWaiter transactionSendingWaiter(m_dispatcher);
      m_wallet->addObserver(&transactionSendingWaiter);

      Transfer transfer{ address, static_cast<int64_t>(amount) };
      auto txId = m_wallet->sendTransaction(transfer, m_currency.minimumFee());
      transactionSendingWaiter.wait(txId);
      m_wallet->removeObserver(&transactionSendingWaiter);
      // TODO workaround: make sure ObserverManager doesn't have local pointers to transactionSendingWaiter, so it can be destroyed
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // Run all spawned handlers from TransactionSendingWaiter::sendTransactionCompleted
      m_dispatcher.yield();

      TransactionInfo txInfo;
      if (!m_wallet->getTransaction(txId, txInfo)) {
        return std::make_error_code(std::errc::identifier_removed);
      }

      txHash = txInfo.hash;
      return transactionSendingWaiter.m_result;
    }

    void waitForSynchronizationToHeight(uint32_t height) {
      while (m_synchronizedHeight < height) {
        m_synchronizationCompleted.wait();
      }
    }

    IWallet* wallet() {
      return m_wallet.get();
    }

    AccountPublicAddress address() const {
      std::string addressString = m_wallet->getAddress();
      AccountPublicAddress address;
      bool ok = m_currency.parseAccountAddressString(addressString, address);
      assert(ok);
      return address;
    }

  protected:
    virtual void synchronizationCompleted(std::error_code result) override {
      m_dispatcher.remoteSpawn([this, result]() {
        m_lastSynchronizationResult = result;
        m_synchronizedHeight = m_currentHeight;
        m_synchronizationCompleted.set();
        m_synchronizationCompleted.clear();
      });
    }

    virtual void synchronizationProgressUpdated(uint64_t current, uint64_t total) override {
      m_dispatcher.remoteSpawn([this, current]() {
        m_currentHeight = static_cast<uint32_t>(current);
      });
    }

  private:
    System::Dispatcher& m_dispatcher;
    System::Event m_synchronizationCompleted;
    System::Event m_someTransactionUpdated;

    INode& m_node;
    Currency& m_currency;
    std::unique_ptr<CryptoNote::IWallet> m_wallet;
    std::unique_ptr<CryptoNote::IWalletObserver> m_walletObserver;
    uint32_t m_currentHeight;
    uint32_t m_synchronizedHeight;
    std::error_code m_lastSynchronizationResult;
  };

  TEST_F(NodeTxPoolSyncTest, TxPoolsAreRequestedRightAfterANodeIsConnectedToAnotherIfTheirBlockchainsAreSynchronized) {
    //System::Timer timer(m_dispatcher);
    //m_dispatcher.spawn([&m_dispatcher, &timer] {
    //  try {
    //    timer.sleep(std::chrono::minutes(5));
    //    m_dispatcher.
    //  } catch (System::InterruptedException&) {
    //  }
    //});

    const size_t NODE_0 = 0;
    const size_t NODE_1 = 1;
    const size_t NODE_2 = 2;
    const size_t NODE_3 = 3;

    launchTestnet(4, Tests::Common::BaseFunctionalTest::Line);

    std::unique_ptr<CryptoNote::INode> node0;
    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> node2;
    std::unique_ptr<CryptoNote::INode> node3;

    nodeDaemons[NODE_0]->makeINode(node0);
    nodeDaemons[NODE_1]->makeINode(node1);
    nodeDaemons[NODE_2]->makeINode(node2);
    nodeDaemons[NODE_3]->makeINode(node3);

    CryptoNote::account_base minerAccount;
    minerAccount.generate();

    TestWallet wallet1(m_dispatcher, m_currency, *node1);
    TestWallet wallet2(m_dispatcher, m_currency, *node2);

    ASSERT_FALSE(static_cast<bool>(wallet1.init()));
    ASSERT_FALSE(static_cast<bool>(wallet2.init()));

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet1.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet2.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], minerAccount.get_keys().m_account_address, m_currency.minedMoneyUnlockWindow()));

    wallet1.waitForSynchronizationToHeight(static_cast<uint32_t>(m_currency.minedMoneyUnlockWindow()) + 3);
    wallet2.waitForSynchronizationToHeight(static_cast<uint32_t>(m_currency.minedMoneyUnlockWindow()) + 3);

    stopNode(NODE_2);
    // To make sure new transaction won't be received by NODE_2 and NODE_3
    waitForPeerCount(*node1, 1);

    TransactionHash txHash1;
    ASSERT_FALSE(static_cast<bool>(wallet1.sendTransaction(m_currency.accountAddressAsString(minerAccount), m_currency.coin(), txHash1)));

    stopNode(NODE_1);
    // Don't start NODE_2, while NODE_1 doesn't close its connections
    waitForPeerCount(*node0, 0);

    startNode(NODE_2);
    waitDaemonReady(NODE_2);
    waitForPeerCount(*node3, 1);

    TransactionHash txHash2;
    ASSERT_FALSE(static_cast<bool>(wallet2.sendTransaction(m_currency.accountAddressAsString(minerAccount), m_currency.coin(), txHash2)));

    startNode(NODE_1);
    waitDaemonReady(NODE_1);

    std::vector<CryptoNote::Transaction> poolTxs1;
    std::vector<CryptoNote::Transaction> poolTxs2;
    ASSERT_TRUE(waitForPoolSize(NODE_1, *node1, 2, poolTxs1));
    ASSERT_TRUE(waitForPoolSize(NODE_2, *node2, 2, poolTxs2));

    //timer.stop();

    std::vector<TransactionHash> poolTxsIds1;
    std::vector<TransactionHash> poolTxsIds2;

    for (auto& tx : poolTxs1) {
      TransactionHash txHash = toTransactionHash(CryptoNote::get_transaction_hash(tx));
      poolTxsIds1.emplace_back(std::move(txHash));
    }
    for (auto& tx : poolTxs2) {
      TransactionHash txHash = toTransactionHash(CryptoNote::get_transaction_hash(tx));
      poolTxsIds2.emplace_back(std::move(txHash));
    }

    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash1) != poolTxsIds1.end());
    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash2) != poolTxsIds1.end());

    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash1) != poolTxsIds2.end());
    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash2) != poolTxsIds2.end());
  }

  TEST_F(NodeTxPoolSyncTest, TxPoolsAreRequestedRightAfterInitialBlockchainsSynchronization) {
    //System::Timer timer(m_dispatcher);
    //m_dispatcher.spawn([&m_dispatcher, &timer] {
    //  try {
    //    timer.sleep(std::chrono::minutes(5));
    //    m_dispatcher.
    //  } catch (System::InterruptedException&) {
    //  }
    //});

    const size_t NODE_0 = 0;
    const size_t NODE_1 = 1;
    const size_t NODE_2 = 2;
    const size_t NODE_3 = 3;

    launchTestnet(4, Tests::Common::BaseFunctionalTest::Line);

    std::unique_ptr<CryptoNote::INode> node0;
    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> node2;
    std::unique_ptr<CryptoNote::INode> node3;

    nodeDaemons[NODE_0]->makeINode(node0);
    nodeDaemons[NODE_1]->makeINode(node1);
    nodeDaemons[NODE_2]->makeINode(node2);
    nodeDaemons[NODE_3]->makeINode(node3);

    CryptoNote::account_base minerAccount;
    minerAccount.generate();

    TestWallet wallet1(m_dispatcher, m_currency, *node1);
    TestWallet wallet2(m_dispatcher, m_currency, *node2);

    ASSERT_FALSE(static_cast<bool>(wallet1.init()));
    ASSERT_FALSE(static_cast<bool>(wallet2.init()));

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet1.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet2.address(), 1));

    wallet1.waitForSynchronizationToHeight(static_cast<uint32_t>(3));
    wallet2.waitForSynchronizationToHeight(static_cast<uint32_t>(3));

    stopNode(NODE_2);
    // To make sure new transaction won't be received by NODE_2 and NODE_3
    waitForPeerCount(*node1, 1);

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], minerAccount.get_keys().m_account_address, m_currency.minedMoneyUnlockWindow()));
    wallet1.waitForSynchronizationToHeight(static_cast<uint32_t>(m_currency.minedMoneyUnlockWindow()) + 3);

    TransactionHash txHash1;
    ASSERT_FALSE(static_cast<bool>(wallet1.sendTransaction(m_currency.accountAddressAsString(minerAccount), m_currency.coin(), txHash1)));

    stopNode(NODE_1);
    // Don't start NODE_2, while NODE_1 doesn't close its connections
    waitForPeerCount(*node0, 0);

    startNode(NODE_2);
    waitDaemonReady(NODE_2);
    waitForPeerCount(*node3, 1);

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_3], minerAccount.get_keys().m_account_address, m_currency.minedMoneyUnlockWindow()));
    wallet2.waitForSynchronizationToHeight(static_cast<uint32_t>(m_currency.minedMoneyUnlockWindow()) + 3);

    TransactionHash txHash2;
    ASSERT_FALSE(static_cast<bool>(wallet2.sendTransaction(m_currency.accountAddressAsString(minerAccount), m_currency.coin(), txHash2)));

    startNode(NODE_1);
    waitDaemonReady(NODE_1);

    std::vector<CryptoNote::Transaction> poolTxs1;
    std::vector<CryptoNote::Transaction> poolTxs2;
    ASSERT_TRUE(waitForPoolSize(NODE_1, *node1, 2, poolTxs1));
    ASSERT_TRUE(waitForPoolSize(NODE_2, *node2, 2, poolTxs2));

    //timer.stop();

    std::vector<TransactionHash> poolTxsIds1;
    std::vector<TransactionHash> poolTxsIds2;

    for (auto& tx : poolTxs1) {
      TransactionHash txHash = toTransactionHash(CryptoNote::get_transaction_hash(tx));
      poolTxsIds1.emplace_back(std::move(txHash));
    }
    for (auto& tx : poolTxs2) {
      TransactionHash txHash = toTransactionHash(CryptoNote::get_transaction_hash(tx));
      poolTxsIds2.emplace_back(std::move(txHash));
    }

    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash1) != poolTxsIds1.end());
    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash2) != poolTxsIds1.end());

    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash1) != poolTxsIds2.end());
    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash2) != poolTxsIds2.end());
  }

  TEST_F(NodeTxPoolSyncTest, TxPoolsAreRequestedRightAfterTimedBlockchainsSynchronization) {
    //System::Timer timer(m_dispatcher);
    //m_dispatcher.spawn([&m_dispatcher, &timer] {
    //  try {
    //    timer.sleep(std::chrono::minutes(5));
    //    m_dispatcher.
    //  } catch (System::InterruptedException&) {
    //  }
    //});

    const size_t NODE_0 = 0;
    const size_t NODE_1 = 1;
    const size_t NODE_2 = 2;
    const size_t NODE_3 = 3;
    const size_t NODE_4 = 4;

    launchTestnet(5, Tests::Common::BaseFunctionalTest::Line);

    std::unique_ptr<CryptoNote::INode> node0;
    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> node2;
    std::unique_ptr<CryptoNote::INode> node3;
    std::unique_ptr<CryptoNote::INode> node4;

    nodeDaemons[NODE_0]->makeINode(node0);
    nodeDaemons[NODE_1]->makeINode(node1);
    nodeDaemons[NODE_2]->makeINode(node2);
    nodeDaemons[NODE_3]->makeINode(node3);
    nodeDaemons[NODE_4]->makeINode(node4);

    CryptoNote::account_base minerAccount;
    minerAccount.generate();

    TestWallet wallet1(m_dispatcher, m_currency, *node1);
    ASSERT_FALSE(static_cast<bool>(wallet1.init()));

    stopNode(NODE_4);
    waitForPeerCount(*node3, 1);

    stopNode(NODE_3);
    waitForPeerCount(*node2, 1);

    stopNode(NODE_2);
    waitForPeerCount(*node1, 1);

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet1.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], minerAccount.get_keys().m_account_address, m_currency.minedMoneyUnlockWindow()));
    wallet1.waitForSynchronizationToHeight(static_cast<uint32_t>(m_currency.minedMoneyUnlockWindow()) + 2);

    TransactionHash txHash1;
    ASSERT_FALSE(static_cast<bool>(wallet1.sendTransaction(m_currency.accountAddressAsString(minerAccount), m_currency.coin(), txHash1)));

    // Start nodes simultaneously due to them connect each other and decided that they are connected to network
    startNode(NODE_4);
    startNode(NODE_3);
    waitDaemonReady(NODE_4);
    waitDaemonReady(NODE_3);
    waitForPeerCount(*node4, 1);
    waitForPeerCount(*node3, 1);

    //std::this_thread::sleep_for(std::chrono::seconds(5));

    startNode(NODE_2);
    waitDaemonReady(NODE_2);

    // NODE_3 and NODE_4 are synchronized by timer
    std::vector<CryptoNote::Transaction> poolTxs2;
    std::vector<CryptoNote::Transaction> poolTxs3;
    std::vector<CryptoNote::Transaction> poolTxs4;
    ASSERT_TRUE(waitForPoolSize(NODE_2, *node2, 1, poolTxs2));
    ASSERT_TRUE(waitForPoolSize(NODE_3, *node3, 1, poolTxs3));
    ASSERT_TRUE(waitForPoolSize(NODE_4, *node4, 1, poolTxs4));

    //timer.stop();

    TransactionHash poolTxId2 = toTransactionHash(CryptoNote::get_transaction_hash(poolTxs2.front()));
    TransactionHash poolTxId3 = toTransactionHash(CryptoNote::get_transaction_hash(poolTxs3.front()));
    TransactionHash poolTxId4 = toTransactionHash(CryptoNote::get_transaction_hash(poolTxs4.front()));

    ASSERT_EQ(txHash1, poolTxId2);
    ASSERT_EQ(txHash1, poolTxId3);
    ASSERT_EQ(txHash1, poolTxId4);
  }

  TEST_F(NodeTxPoolSyncTest, TxPoolsAreRequestedRightAfterSwitchingToAlternativeChain) {
    // If this condition isn't true, then test must be rewritten a bit
    ASSERT_GT(m_currency.difficultyLag() + m_currency.difficultyCut(), m_currency.minedMoneyUnlockWindow());

    //System::Timer timer(m_dispatcher);
    //m_dispatcher.spawn([&m_dispatcher, &timer] {
    //  try {
    //    timer.sleep(std::chrono::minutes(5));
    //    m_dispatcher.
    //  } catch (System::InterruptedException&) {
    //  }
    //});

    const size_t NODE_0 = 0;
    const size_t NODE_1 = 1;
    const size_t NODE_2 = 2;
    const size_t NODE_3 = 3;
    const size_t NODE_4 = 4;
    const size_t NODE_5 = 5;

    launchTestnet(6, Tests::Common::BaseFunctionalTest::Line);

    std::unique_ptr<CryptoNote::INode> node0;
    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> node2;
    std::unique_ptr<CryptoNote::INode> node3;
    std::unique_ptr<CryptoNote::INode> node4;
    std::unique_ptr<CryptoNote::INode> node5;

    nodeDaemons[NODE_0]->makeINode(node0);
    nodeDaemons[NODE_1]->makeINode(node1);
    nodeDaemons[NODE_2]->makeINode(node2);
    nodeDaemons[NODE_3]->makeINode(node3);
    nodeDaemons[NODE_4]->makeINode(node4);
    nodeDaemons[NODE_5]->makeINode(node5);

    TestWallet wallet0(m_dispatcher, m_currency, *node1);
    TestWallet wallet1(m_dispatcher, m_currency, *node1);
    TestWallet wallet2(m_dispatcher, m_currency, *node2);
    TestWallet wallet5(m_dispatcher, m_currency, *node5);

    ASSERT_FALSE(static_cast<bool>(wallet0.init()));
    ASSERT_FALSE(static_cast<bool>(wallet1.init()));
    ASSERT_FALSE(static_cast<bool>(wallet2.init()));
    ASSERT_FALSE(static_cast<bool>(wallet5.init()));

    uint32_t blockchainLenght = 1;
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet0.address(), m_currency.difficultyBlocksCount()));
    blockchainLenght += static_cast<uint32_t>(m_currency.difficultyBlocksCount());

    wallet1.waitForSynchronizationToHeight(blockchainLenght);
    wallet2.waitForSynchronizationToHeight(blockchainLenght);
    wallet5.waitForSynchronizationToHeight(blockchainLenght);

    stopNode(NODE_2);
    // To make sure new blocks won't be received by NODE_2
    waitForPeerCount(*node1, 1);

    // Generate alternative chain for NODE_1
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet1.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet2.address(), m_currency.minedMoneyUnlockWindow()));
    blockchainLenght += 1 + static_cast<uint32_t>(m_currency.minedMoneyUnlockWindow());

    wallet1.waitForSynchronizationToHeight(blockchainLenght);

    // This transaction is valid in both alternative chains, it is just an indicator, that shows when NODE_1 and NODE_2 are synchronized
    TransactionHash txHash0;
    ASSERT_FALSE(static_cast<bool>(wallet0.sendTransaction(wallet0.wallet()->getAddress(), m_currency.coin(), txHash0)));

    // This transaction is valid only in alternative chain 1
    TransactionHash txHash1;
    ASSERT_FALSE(static_cast<bool>(wallet1.sendTransaction(wallet0.wallet()->getAddress(), m_currency.coin(), txHash1)));

    stopNode(NODE_1);
    // Don't start NODE_2, while NODE_1 doesn't close its connections
    waitForPeerCount(*node0, 0);

    startNode(NODE_2);
    waitDaemonReady(NODE_2);
    waitForPeerCount(*node3, 1);

    // Generate alternative chain for NODE_2.
    // After that it is expected that alternative chains 1 and 2 have the same difficulty, because
    // m_currency.minedMoneyUnlockWindow() < m_currency.difficultyLag() + m_currency.difficultyCut()
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_5], wallet2.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_5], wallet1.address(), m_currency.minedMoneyUnlockWindow()));

    wallet2.waitForSynchronizationToHeight(blockchainLenght);
    wallet5.waitForSynchronizationToHeight(blockchainLenght);

    // Tear connection between NODE_2 and nodes 4 and 5, in order to this nodes doesn't receive new transactions
    stopNode(NODE_3);
    waitForPeerCount(*node4, 1);

    // This transaction is valid only in alternative chain 2
    TransactionHash txHash2;
    ASSERT_FALSE(static_cast<bool>(wallet2.sendTransaction(wallet0.wallet()->getAddress(), m_currency.coin(), txHash2)));

    startNode(NODE_1);
    waitDaemonReady(NODE_1);
    waitForPeerCount(*node2, 1);

    std::vector<CryptoNote::Transaction> poolTxs2;
    ASSERT_TRUE(waitForPoolSize(NODE_2, *node2, 2, poolTxs2));

    // Now NODE_1 and NODE_2 are synchronized, but both are on its own alternative chains
    // Add block to alternative chain 2, and wait for when NODE_1 switches to alternative chain 2.
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_5], wallet1.address(), 1));
    blockchainLenght += 1;

    startNode(NODE_3);
    waitDaemonReady(NODE_3);
    waitForPeerCount(*node2, 2);

    wallet1.waitForSynchronizationToHeight(blockchainLenght);
    wallet2.waitForSynchronizationToHeight(blockchainLenght);

    std::vector<CryptoNote::Transaction> poolTxs1;
    ASSERT_TRUE(waitForPoolSize(NODE_1, *node1, 2, poolTxs1));
    ASSERT_TRUE(waitForPoolSize(NODE_2, *node2, 2, poolTxs2));

    //timer.stop();

    std::vector<TransactionHash> poolTxsIds1;
    std::vector<TransactionHash> poolTxsIds2;

    for (auto& tx : poolTxs1) {
      TransactionHash txHash = toTransactionHash(CryptoNote::get_transaction_hash(tx));
      poolTxsIds1.emplace_back(std::move(txHash));
    }
    for (auto& tx : poolTxs2) {
      TransactionHash txHash = toTransactionHash(CryptoNote::get_transaction_hash(tx));
      poolTxsIds2.emplace_back(std::move(txHash));
    }

    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash0) != poolTxsIds1.end());
    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash2) != poolTxsIds1.end());

    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash0) != poolTxsIds2.end());
    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash2) != poolTxsIds2.end());
  }
}
