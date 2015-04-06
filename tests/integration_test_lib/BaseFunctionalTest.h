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

#pragma once

#include <list>
#include <vector>
#include <memory>
#include <mutex>              
#include <condition_variable> 
#include <queue>

#include <boost/noncopyable.hpp>
#include <boost/program_options.hpp>

#include "TestNode.h"
#include <System/Dispatcher.h>
#include "cryptonote_core/Currency.h"
#include "inprocess_node/InProcessNode.h"

#include "../../cryptonote_core/cryptonote_core.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "p2p/net_node.h"

#include "IWallet.h"
#include "INode.h"

namespace Tests {
  namespace Common {

  namespace po = boost::program_options;
    class Semaphore{
    private:
      std::mutex mtx;
      std::condition_variable cv;
      bool available;

    public:
      Semaphore() : available(false) { }

      void notify() {
        std::unique_lock<std::mutex> lck(mtx);
        available = true;
        cv.notify_one();
      }

      void wait() {
        std::unique_lock<std::mutex> lck(mtx);
        cv.wait(lck, [this](){ return available; });
        available = false;
      }

      bool wait_for(const std::chrono::milliseconds& rel_time) {
        std::unique_lock<std::mutex> lck(mtx);
        auto result = cv.wait_for(lck, rel_time, [this](){ return available; });
        available = false;
        return result;
      }
    };

    const uint16_t P2P_FIRST_PORT = 8000;
    const uint16_t RPC_FIRST_PORT = 8200;


    class BaseFunctionalTestConfig {
    public:
      BaseFunctionalTestConfig() {}

      void init(po::options_description& desc) {
        desc.add_options()
          ("daemon-dir,d", po::value<std::string>()->default_value("."), "path to bytecoind.exe")
          ("data-dir,n", po::value<std::string>()->default_value("."), "path to daemon's data directory");
      }

      bool handleCommandLine(const po::variables_map& vm) {
        if (vm.count("daemon-dir")) {
          daemonDir = vm["daemon-dir"].as<std::string>();
        }

        if (vm.count("data-dir")) {
          dataDir = vm["data-dir"].as<std::string>();
        }
        return true;
      }


    protected:
      friend class BaseFunctionalTest;

      std::string daemonDir;
      std::string dataDir;
    };



    class BaseFunctionalTest : boost::noncopyable {
    public:
      BaseFunctionalTest(const cryptonote::Currency& currency, System::Dispatcher& d, const BaseFunctionalTestConfig& config) : m_currency(currency), m_dataDir(config.dataDir), m_daemonDir(config.daemonDir), m_dispatcher(d), inprocNode(nullptr) {
        if (m_dataDir.empty()) m_dataDir = ".";
        if (m_daemonDir.empty()) m_daemonDir = ".";
      };

      ~BaseFunctionalTest(); 

      enum Topology {
        Ring,
        Line,
        Star
      };

    private:
      std::unique_ptr<cryptonote::core> core;
      std::unique_ptr<cryptonote::t_cryptonote_protocol_handler<cryptonote::core>> protocol;
      std::unique_ptr<nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core>>> p2pNode;

    protected:
      std::vector< std::unique_ptr<TestNode> > nodeDaemons;
      System::Dispatcher& m_dispatcher;
      const cryptonote::Currency& m_currency;
      std::unique_ptr<CryptoNote::INode> inprocNode;

      void launchTestnet(size_t count, Topology t = Line);
      void launchTestnetWithInprocNode(size_t count, Topology t = Line);
      void stopTestnet();
      bool makeWallet(std::unique_ptr<CryptoNote::IWallet> & wallet, std::unique_ptr<CryptoNote::INode>& node, const std::string& password = "pass");
      bool mineBlock(std::unique_ptr<CryptoNote::IWallet>& wallet);
      bool mineBlock();
      bool startMining(size_t threads);
      bool stopMining();

    private:
#ifdef __linux__
      std::vector<__pid_t> pids;
#endif

      cryptonote::CurrencyBuilder currencyBuilder;
      std::unique_ptr<CryptoNote::INode> mainNode;
      std::unique_ptr<CryptoNote::IWallet> workingWallet;
      

      std::string m_dataDir;
      std::string m_daemonDir;
      uint16_t m_mainDaemonRPCPort;  
    };
  }
}
