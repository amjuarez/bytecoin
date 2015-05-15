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
