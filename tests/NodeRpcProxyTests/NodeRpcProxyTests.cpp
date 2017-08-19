// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chrono>
#include <thread>

#include <Logging/LoggerRef.h>
#include <Logging/ConsoleLogger.h>

#include "NodeRpcProxy/NodeRpcProxy.h"

using namespace CryptoNote;
using namespace Logging;

#undef ERROR

class NodeObserver : public INodeObserver {
public:
  NodeObserver(const std::string& name, NodeRpcProxy& nodeProxy, ILogger& log)
    : m_name(name)
    , m_nodeProxy(nodeProxy)
    , logger(log, "NodeObserver:" + name) {
  }

  virtual ~NodeObserver() {
  }

  virtual void peerCountUpdated(size_t count) override {
    logger(INFO) << '[' << m_name << "] peerCountUpdated " << count << " = " << m_nodeProxy.getPeerCount();
  }

  virtual void localBlockchainUpdated(uint32_t height) override {
    logger(INFO) << '[' << m_name << "] localBlockchainUpdated " << height << " = " << m_nodeProxy.getLastLocalBlockHeight();

    std::vector<uint64_t> amounts;
    amounts.push_back(100000000);
    auto outs = std::make_shared<std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>>();
    m_nodeProxy.getRandomOutsByAmounts(std::move(amounts), 10, *outs.get(), [outs, this](std::error_code ec) {
      if (!ec) {
        if (1 == outs->size() && 10 == (*outs)[0].outs.size()) {
          logger(INFO) << "getRandomOutsByAmounts called successfully";
        } else {
          logger(ERROR) << "getRandomOutsByAmounts returned invalid result";
        }
      } else {
        logger(ERROR) << "failed to call getRandomOutsByAmounts: " << ec.message() << ':' << ec.value();
      }
    });
  }

  virtual void lastKnownBlockHeightUpdated(uint32_t height) override {
    logger(INFO) << '[' << m_name << "] lastKnownBlockHeightUpdated " << height << " = " << m_nodeProxy.getLastKnownBlockHeight();
  }

private:
  LoggerRef logger;
  std::string m_name;
  NodeRpcProxy& m_nodeProxy;
};

int main(int argc, const char** argv) {

  Logging::ConsoleLogger log;
  Logging::LoggerRef logger(log, "main");
  NodeRpcProxy nodeProxy("127.0.0.1", 18081);

  NodeObserver observer1("obs1", nodeProxy, log);
  NodeObserver observer2("obs2", nodeProxy, log);

  nodeProxy.addObserver(&observer1);
  nodeProxy.addObserver(&observer2);

  nodeProxy.init([&](std::error_code ec) {
    if (ec) {
      logger(ERROR) << "init error: " << ec.message() << ':' << ec.value();
    } else {
      logger(INFO, BRIGHT_GREEN) << "initialized";
    }
  });

  //nodeProxy.init([](std::error_code ec) {
  //  if (ec) {
  //    logger(ERROR) << "init error: " << ec.message() << ':' << ec.value();
  //  } else {
  //    LOG_PRINT_GREEN("initialized", LOG_LEVEL_0);
  //  }
  //});

  std::this_thread::sleep_for(std::chrono::seconds(5));
  if (nodeProxy.shutdown()) {
    logger(INFO, BRIGHT_GREEN) << "shutdown";
  } else {
    logger(ERROR) << "shutdown error";
  }

  nodeProxy.init([&](std::error_code ec) {
    if (ec) {
      logger(ERROR) << "init error: " << ec.message() << ':' << ec.value();
    } else {
      logger(INFO, BRIGHT_GREEN) << "initialized";
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(5));
  if (nodeProxy.shutdown()) {
    logger(INFO, BRIGHT_GREEN) << "shutdown";
  } else {
    logger(ERROR) << "shutdown error";
  }

  CryptoNote::Transaction tx;
  nodeProxy.relayTransaction(tx, [&](std::error_code ec) {
    if (!ec) {
      logger(INFO) << "relayTransaction called successfully";
    } else {
      logger(ERROR) << "failed to call relayTransaction: " << ec.message() << ':' << ec.value();
    }
  });

  nodeProxy.init([&](std::error_code ec) {
    if (ec) {
      logger(ERROR) << "init error: " << ec.message() << ':' << ec.value();
    } else {
      logger(INFO, BRIGHT_GREEN) << "initialized";
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(5));
  nodeProxy.relayTransaction(tx, [&](std::error_code ec) {
    if (!ec) {
      logger(INFO) << "relayTransaction called successfully";
    } else {
      logger(ERROR) << "failed to call relayTransaction: " << ec.message() << ':' << ec.value();
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(60));
}
