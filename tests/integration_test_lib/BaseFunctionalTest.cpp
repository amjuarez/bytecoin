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

#include "BaseFunctionalTest.h"

#include <vector>
#include <thread>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#include "p2p/NetNodeConfig.h"
#include "cryptonote_core/CoreConfig.h"

#include "RPCTestNode.h"
#include "wallet/Wallet.h"
#include "Logger.h"

#if defined __linux__
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#endif

using namespace Tests::Common;

void BaseFunctionalTest::launchTestnet(size_t count, Topology t) {
  if (count < 1) LOG_WARNING("Testnet has no nodes");
  for (uint16_t i = 0; i < count; ++i) {
    std::string dataDirPath = m_dataDir + "/node";
    dataDirPath += boost::lexical_cast<std::string>(i);
    boost::filesystem::create_directory(dataDirPath);

    std::ofstream config(dataDirPath + "/daemon.conf", std::ios_base::trunc | std::ios_base::out);

    uint16_t rpcPort = RPC_FIRST_PORT + i;
    uint16_t p2pPort = P2P_FIRST_PORT + i;

    config
      << "rpc-bind-port=" << rpcPort << std::endl
      << "p2p-bind-port=" << p2pPort << std::endl
      << "log-level=2" << std::endl
      << "log-file=test_bytecoind_" << i + 1 << ".log" << std::endl;

    switch (t) {
    case Line:
      if (i != count - 1) config << "add-exclusive-node=127.0.0.1:" << p2pPort + 1 << std::endl;
      if (i != 0)         config << "add-exclusive-node=127.0.0.1:" << p2pPort - 1 << std::endl;
      break;
    case Ring: {
      uint16_t p2pExternalPort = P2P_FIRST_PORT + (i + 1) % count;
      config << "add-exclusive-node=127.0.0.1:" << p2pExternalPort + 1 << std::endl;
    }
      break;
    case Star:
      if (i == 0) {
        for (size_t node = 1; node < count; ++node)
          config << "add-exclusive-node=127.0.0.1:" << P2P_FIRST_PORT + node << std::endl;
      }
      else {
        config << "add-exclusive-node=127.0.0.1:" << P2P_FIRST_PORT << std::endl;
      }
      break;
    }
    config.close();
#if defined WIN32
    std::string commandLine = "start /MIN \"bytecoind\" \"" + m_daemonDir + "\\bytecoind.exe\" --testnet --data-dir=\"" + dataDirPath + "\" --config-file=daemon.conf";
    LOG_DEBUG(commandLine);
    system(commandLine.c_str());
#elif defined __linux__
    auto pid = fork();
    if(  pid == 0 ) {
        std::string pathToDaemon = "" + m_daemonDir + "/bytecoind";
        close(1);
        close(2);
        std::string dataDir = "--data-dir=" + dataDirPath + "";
        if(execl(pathToDaemon.c_str(), "bytecoind", "--testnet", dataDir.c_str(), "--config-file=daemon.conf", NULL) == -1) {
            LOG_ERROR(TO_STRING(errno));
        }
        throw std::runtime_error("failed to start daemon");
    } else if(pid > 0) {
        pids.push_back(pid);
    }
#else

#endif

    nodeDaemons.push_back(
      std::unique_ptr<TestNode>(new RPCTestNode(rpcPort, m_dispatcher))
      );
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(10000)); //for initial update
  nodeDaemons[0]->makeINode(mainNode);
  makeWallet(workingWallet, mainNode);
}

void BaseFunctionalTest::launchTestnetWithInprocNode(size_t count, Topology t) {
  if (count < 1) LOG_WARNING("Testnet has no nodes");
  for (uint16_t i = 0; i < count-1; ++i) {
    std::string dataDirPath = m_dataDir + "/node";
    dataDirPath += boost::lexical_cast<std::string>(i);
    boost::filesystem::create_directory(dataDirPath);

    std::ofstream config(dataDirPath + "/daemon.conf", std::ios_base::trunc | std::ios_base::out);

    uint16_t rpcPort = RPC_FIRST_PORT + i;
    uint16_t p2pPort = P2P_FIRST_PORT + i;

    config
      << "rpc-bind-port=" << rpcPort << std::endl
      << "p2p-bind-port=" << p2pPort << std::endl
      << "log-level=2" << std::endl
      << "log-file=test_bytecoind_" << i + 1 << ".log" << std::endl;

    switch (t) {
    case Line:
      config << "add-exclusive-node=127.0.0.1:" << p2pPort + 1 << std::endl;
      if (i != 0)         config << "add-exclusive-node=127.0.0.1:" << p2pPort - 1 << std::endl;
      break;
    case Ring: {
      uint16_t p2pExternalPort = P2P_FIRST_PORT + (i + 1) % count;
      config << "add-exclusive-node=127.0.0.1:" << p2pExternalPort + 1 << std::endl;
    }
      break;
    case Star:
      if (i == 0) {
        for (size_t node = 1; node < count; ++node)
          config << "add-exclusive-node=127.0.0.1:" << P2P_FIRST_PORT + node << std::endl;
      } else {
        config << "add-exclusive-node=127.0.0.1:" << P2P_FIRST_PORT << std::endl;
      }
      break;
    }
    config.close();
#if defined WIN32
    std::string commandLine = "start /MIN \"bytecoind\" \"" + m_daemonDir + "\\bytecoind.exe\" --testnet --data-dir=\"" + dataDirPath + "\" --config-file=daemon.conf";
    LOG_DEBUG(commandLine);
    system(commandLine.c_str());
#elif defined __linux__
    auto pid = fork();
    if (pid == 0) {
      std::string pathToDaemon = "" + m_daemonDir + "/bytecoind";
      close(1);
      close(2);
      std::string dataDir = "--data-dir=" + dataDirPath + "";
      if (execl(pathToDaemon.c_str(), "bytecoind", "--testnet", dataDir.c_str(), "--config-file=daemon.conf", NULL) == -1) {
        LOG_ERROR(TO_STRING(errno));
      }
      throw std::runtime_error("failed to start daemon");
    } else if (pid > 0) {
      pids.push_back(pid);
    }
#else

#endif

    nodeDaemons.push_back(
      std::unique_ptr<TestNode>(new RPCTestNode(rpcPort, m_dispatcher))
      );
  }
    
  this->core.reset(new cryptonote::core(m_currency, NULL));
  this->protocol.reset(new cryptonote::t_cryptonote_protocol_handler<cryptonote::core>(*core, NULL));
  this->p2pNode.reset(new nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core>>(*protocol));
  protocol->set_p2p_endpoint(p2pNode.get());
  core->set_cryptonote_protocol(protocol.get());

  std::string dataDirPath = m_dataDir + "/node";
  dataDirPath += boost::lexical_cast<std::string>(count - 1);
  boost::filesystem::create_directory(dataDirPath);

  uint16_t p2pPort = P2P_FIRST_PORT + static_cast<uint16_t>(count) - 1;

  nodetool::NetNodeConfig p2pConfig;
  p2pConfig.bindIp = "127.0.0.1";
  p2pConfig.bindPort = boost::lexical_cast<std::string>(p2pPort);
  nodetool::net_address addr;
  addr.ip = 0x7f000001;

  p2pConfig.externalPort = 0;
  p2pConfig.allowLocalIp = false;
  p2pConfig.hideMyPort = false;
  p2pConfig.configFolder = dataDirPath;


  switch (t) {
  case Line:
    addr.port = p2pPort - 1;
    p2pConfig.exclusiveNodes.push_back(addr);
    break;
  case Ring:
    addr.port = p2pPort - 1;
    p2pConfig.exclusiveNodes.push_back(addr);
    addr.port = P2P_FIRST_PORT;
    p2pConfig.exclusiveNodes.push_back(addr);
    break;
  case Star:
    addr.port = P2P_FIRST_PORT;
    p2pConfig.exclusiveNodes.push_back(addr);
    break;
  }

  if (!p2pNode->init(p2pConfig, true)) {
    throw std::runtime_error("Failed to init p2pNode");
  }

  protocol->init();

  cryptonote::MinerConfig emptyMiner;
  cryptonote::CoreConfig coreConfig;
  coreConfig.configFolder = dataDirPath;
  core->init(coreConfig, emptyMiner, true);

  inprocNode.reset(new CryptoNote::InProcessNode(*core, *protocol));
  std::promise<void> p;
  auto future = p.get_future();
  inprocNode->init([&p](std::error_code ec) {
    p.set_value();
    if (ec) {
      std::cout << ec.message() << std::endl;
    } 
  });

  future.get();
  
  std::thread serverThread(
    std::bind(
    &nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core>>::run,
    p2pNode.get()
    )
    );
  serverThread.detach();



  std::this_thread::sleep_for(std::chrono::milliseconds(10000)); //for initial update
  nodeDaemons[0]->makeINode(mainNode);
  makeWallet(workingWallet, mainNode);




}


BaseFunctionalTest::~BaseFunctionalTest() {
  if (mainNode) {
    mainNode->shutdown();
  }

  if (inprocNode) {
    inprocNode->shutdown();
  }

  if (p2pNode) {
    p2pNode->send_stop_signal();
  }

  std::this_thread::sleep_for(std::chrono::seconds(2));
  stopTestnet();
}

namespace {
  class WaitForCoinBaseObserver : public CryptoNote::IWalletObserver {
    Semaphore& m_gotReward;
    CryptoNote::IWallet& m_wallet;
  public:
    WaitForCoinBaseObserver(Semaphore& gotReward, CryptoNote::IWallet& wallet) : m_gotReward(gotReward), m_wallet(wallet) { }
    virtual void externalTransactionCreated(CryptoNote::TransactionId transactionId) override {
      CryptoNote::TransactionInfo trInfo;
      m_wallet.getTransaction(transactionId, trInfo);
      if (trInfo.isCoinbase) m_gotReward.notify();
    }
  };
}

bool BaseFunctionalTest::mineBlock(std::unique_ptr<CryptoNote::IWallet>& wallet) {
  if (nodeDaemons.empty() || !wallet) return false;
  if (!nodeDaemons.front()->stopMining()) return false;
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
  Semaphore gotReward;
  WaitForCoinBaseObserver cbo(gotReward, *wallet.get());
  wallet->addObserver(&cbo);
  if(!nodeDaemons.front()->startMining(1, wallet->getAddress())) return false;
  gotReward.wait();
  if (!nodeDaemons.front()->stopMining()) return false;
  wallet->removeObserver(&cbo);
  return true;
}
bool BaseFunctionalTest::mineBlock() {
  return mineBlock(workingWallet);
}

bool BaseFunctionalTest::startMining(size_t threads) {
  if (nodeDaemons.empty() || !workingWallet) return false;
  if(!stopMining()) return false;
  return nodeDaemons.front()->startMining(threads, workingWallet->getAddress());
}

bool BaseFunctionalTest::stopMining() {
  if (nodeDaemons.empty()) return false;
  return nodeDaemons.front()->stopMining();
}

bool BaseFunctionalTest::makeWallet(std::unique_ptr<CryptoNote::IWallet> & wallet, std::unique_ptr<CryptoNote::INode>& node, const std::string& password) {
  if (!node) return false;
  wallet = std::unique_ptr<CryptoNote::IWallet>(new CryptoNote::Wallet(m_currency, *node));
  wallet->initAndGenerate(password);
  return true;
}

void BaseFunctionalTest::stopTestnet() {
  for (auto& Daemon : nodeDaemons) {
    Daemon->stopDaemon();
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(5000));
#ifdef __linux__
  for (auto& pid : pids) {
      int status;
      while (-1 == waitpid(pid, &status, 0));
      if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
          std::cerr << "Process " << " (pid " << pid << ") failed" << std::endl;
          exit(1);
      }
  }
#endif
}
