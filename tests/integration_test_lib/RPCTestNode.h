// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <System/Dispatcher.h>
#include "HTTP/HttpRequest.h"
#include "HTTP/HttpResponse.h"


#include "TestNode.h"

using namespace cryptonote;

namespace Tests {
  class RPCTestNode : public Common::TestNode {
  public:
    RPCTestNode(uint16_t port, System::Dispatcher& d) : m_rpcPort(port), m_dispatcher(d) {}
    virtual bool startMining(size_t threadsCount, const std::string& address) override;
    virtual bool stopMining() override;
    virtual bool stopDaemon() override;
    virtual bool submitBlock(const std::string& block) override;
    virtual bool makeINode(std::unique_ptr<CryptoNote::INode>& node) override;
    virtual ~RPCTestNode() { }

  private:
    void prepareRequest(HttpRequest& httpReq, const std::string& method, const std::string& params);
    void sendRequest(const HttpRequest& httpReq, HttpResponse& httpResp);

    uint16_t m_rpcPort;
    System::Dispatcher& m_dispatcher;
  };
}
