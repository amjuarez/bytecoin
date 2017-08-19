// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <queue>

#include "IP2pNodeInternal.h"
#include "LevinProtocol.h"
#include "P2pContextOwner.h"
#include "P2pInterfaces.h"

namespace CryptoNote {

class P2pContext;
class P2pNode;

class P2pConnectionProxy : public IP2pConnection {
public:

  P2pConnectionProxy(P2pContextOwner&& ctx, IP2pNodeInternal& node);
  ~P2pConnectionProxy();

  bool processIncomingHandshake();

  // IP2pConnection
  virtual void read(P2pMessage& message) override;
  virtual void write(const P2pMessage &message) override;
  virtual void ban() override;
  virtual void stop() override;

private:

  void writeHandshake(const P2pMessage &message);
  void handleHandshakeRequest(const LevinProtocol::Command& cmd);
  void handleHandshakeResponse(const LevinProtocol::Command& cmd, P2pMessage& message);
  void handleTimedSync(const LevinProtocol::Command& cmd);

  std::queue<P2pMessage> m_readQueue;
  P2pContextOwner m_contextOwner;
  P2pContext& m_context;
  IP2pNodeInternal& m_node;
};

}
