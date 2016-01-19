// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include "Logging/LoggerManager.h"
#include "System/Dispatcher.h"
#include "System/InterruptedException.h"

#include "../IntegrationTestLib/BaseFunctionalTests.h"
#include "../IntegrationTestLib/TestWalletLegacy.h"


using namespace CryptoNote;
using namespace Crypto;
using namespace Tests::Common;

extern System::Dispatcher globalSystem;
extern Tests::Common::BaseFunctionalTestsConfig config;

namespace {
  class NodeRpcProxyTest : public Tests::Common::BaseFunctionalTests, public ::testing::Test {
  public:
    NodeRpcProxyTest() :
      BaseFunctionalTests(m_currency, globalSystem, config),
      m_currency(CurrencyBuilder(m_logManager).testnet(true).currency()) {
    }

  protected:
    Logging::LoggerManager m_logManager;
    CryptoNote::Currency m_currency;
  };

  class PoolChangedObserver : public INodeObserver {
  public:
    virtual void poolChanged() override {
      std::unique_lock<std::mutex> lk(mutex);
      ready = true;
      cv.notify_all();
    }

    bool waitPoolChanged(size_t seconds) {
      std::unique_lock<std::mutex> lk(mutex);
      bool r = cv.wait_for(lk, std::chrono::seconds(seconds), [this]() { return ready; });
      ready = false;
      return r;
    }

  private:
    std::mutex mutex;
    std::condition_variable cv;
    bool ready = false;
  };

  TEST_F(NodeRpcProxyTest, PoolChangedCalledWhenTxCame) {
    const size_t NODE_0 = 0;
    const size_t NODE_1 = 1;

    launchTestnet(2, Tests::Common::BaseFunctionalTests::Line);

    std::unique_ptr<CryptoNote::INode> node0;
    std::unique_ptr<CryptoNote::INode> node1;

    nodeDaemons[NODE_0]->makeINode(node0);
    nodeDaemons[NODE_1]->makeINode(node1);

    TestWalletLegacy wallet1(m_dispatcher, m_currency, *node0);
    TestWalletLegacy wallet2(m_dispatcher, m_currency, *node1);

    ASSERT_FALSE(static_cast<bool>(wallet1.init()));
    ASSERT_FALSE(static_cast<bool>(wallet2.init()));

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet1.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet1.address(), m_currency.minedMoneyUnlockWindow()));

    wallet1.waitForSynchronizationToHeight(static_cast<uint32_t>(m_currency.minedMoneyUnlockWindow()) + 1);
    wallet2.waitForSynchronizationToHeight(static_cast<uint32_t>(m_currency.minedMoneyUnlockWindow()) + 1);

    PoolChangedObserver observer;
    node0->addObserver(&observer);

    Hash dontCare;
    ASSERT_FALSE(static_cast<bool>(wallet1.sendTransaction(m_currency.accountAddressAsString(wallet2.address()), m_currency.coin(), dontCare)));
    ASSERT_TRUE(observer.waitPoolChanged(10));
  }
}
