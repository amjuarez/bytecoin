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

#include <chrono>
#include <thread>

#include "include_base_utils.h"

#include "node_rpc_proxy/NodeRpcProxy.h"

using namespace cryptonote;
using namespace CryptoNote;


class NodeObserver : public INodeObserver {
public:
  NodeObserver(const std::string& name, NodeRpcProxy& nodeProxy)
    : m_name(name)
    , m_nodeProxy(nodeProxy) {
  }

  virtual ~NodeObserver() {
  }

  virtual void peerCountUpdated(size_t count) {
    LOG_PRINT_L0('[' << m_name << "] peerCountUpdated " << count << " = " << m_nodeProxy.getPeerCount());
  }

  virtual void localBlockchainUpdated(uint64_t height) {
    LOG_PRINT_L0('[' << m_name << "] localBlockchainUpdated " << height << " = " << m_nodeProxy.getLastLocalBlockHeight());

    std::vector<uint64_t> amounts;
    amounts.push_back(100000000);
    auto outs = std::make_shared<std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>>();
    m_nodeProxy.getRandomOutsByAmounts(std::move(amounts), 10, *outs.get(), [outs](std::error_code ec) {
      if (!ec) {
        if (1 == outs->size() && 10 == (*outs)[0].outs.size()) {
          LOG_PRINT_L0("getRandomOutsByAmounts called successfully");
        } else {
          LOG_PRINT_RED_L0("getRandomOutsByAmounts returned invalid result");
        }
      } else {
        LOG_PRINT_RED_L0("failed to call getRandomOutsByAmounts: " << ec.message() << ':' << ec.value());
      }
    });
  }

  virtual void lastKnownBlockHeightUpdated(uint64_t height) {
    LOG_PRINT_L0('[' << m_name << "] lastKnownBlockHeightUpdated " << height << " = " << m_nodeProxy.getLastKnownBlockHeight());
  }

private:
  std::string m_name;
  NodeRpcProxy& m_nodeProxy;
};

int main(int argc, const char** argv) {
  //set up logging options
  epee::log_space::get_set_log_detalisation_level(true, LOG_LEVEL_2);
  epee::log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL);

  NodeRpcProxy nodeProxy("127.0.0.1", 18081);

  NodeObserver observer1("obs1", nodeProxy);
  NodeObserver observer2("obs2", nodeProxy);

  nodeProxy.addObserver(&observer1);
  nodeProxy.addObserver(&observer2);

  nodeProxy.init([](std::error_code ec) {
    if (ec) {
      LOG_PRINT_RED_L0("init error: " << ec.message() << ':' << ec.value());
    } else {
      LOG_PRINT_GREEN("initialized", LOG_LEVEL_0);
    }
  });

  //nodeProxy.init([](std::error_code ec) {
  //  if (ec) {
  //    LOG_PRINT_RED_L0("init error: " << ec.message() << ':' << ec.value());
  //  } else {
  //    LOG_PRINT_GREEN("initialized", LOG_LEVEL_0);
  //  }
  //});

  std::this_thread::sleep_for(std::chrono::seconds(5));
  if (nodeProxy.shutdown()) {
    LOG_PRINT_GREEN("shutdown", LOG_LEVEL_0);
  } else {
    LOG_PRINT_RED_L0("shutdown error");
  }

  nodeProxy.init([](std::error_code ec) {
    if (ec) {
      LOG_PRINT_RED_L0("init error: " << ec.message() << ':' << ec.value());
    } else {
      LOG_PRINT_GREEN("initialized", LOG_LEVEL_0);
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(5));
  if (nodeProxy.shutdown()) {
    LOG_PRINT_GREEN("shutdown", LOG_LEVEL_0);
  } else {
    LOG_PRINT_RED_L0("shutdown error");
  }

  cryptonote::Transaction tx;
  nodeProxy.relayTransaction(tx, [](std::error_code ec) {
    if (!ec) {
      LOG_PRINT_L0("relayTransaction called successfully");
    } else {
      LOG_PRINT_RED_L0("failed to call relayTransaction: " << ec.message() << ':' << ec.value());
    }
  });

  nodeProxy.init([](std::error_code ec) {
    if (ec) {
      LOG_PRINT_RED_L0("init error: " << ec.message() << ':' << ec.value());
    } else {
      LOG_PRINT_GREEN("initialized", LOG_LEVEL_0);
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(5));
  nodeProxy.relayTransaction(tx, [](std::error_code ec) {
    if (!ec) {
      LOG_PRINT_L0("relayTransaction called successfully");
    } else {
      LOG_PRINT_RED_L0("failed to call relayTransaction: " << ec.message() << ':' << ec.value());
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(60));
}
