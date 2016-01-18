// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <chrono>
#include "NetNodeConfig.h"

namespace CryptoNote {

class P2pNodeConfig : public NetNodeConfig {
public:
  P2pNodeConfig();

  // getters
  std::chrono::nanoseconds getTimedSyncInterval() const;
  std::chrono::nanoseconds getHandshakeTimeout() const;
  std::chrono::nanoseconds getConnectInterval() const;
  std::chrono::nanoseconds getConnectTimeout() const;
  size_t getExpectedOutgoingConnectionsCount() const;
  size_t getWhiteListConnectionsPercent() const;
  boost::uuids::uuid getNetworkId() const;
  size_t getPeerListConnectRange() const;
  size_t getPeerListGetTryCount() const;

  // setters
  void setTimedSyncInterval(std::chrono::nanoseconds interval);
  void setHandshakeTimeout(std::chrono::nanoseconds timeout);
  void setConnectInterval(std::chrono::nanoseconds interval);
  void setConnectTimeout(std::chrono::nanoseconds timeout);
  void setExpectedOutgoingConnectionsCount(size_t count);
  void setWhiteListConnectionsPercent(size_t percent);
  void setNetworkId(const boost::uuids::uuid& id);
  void setPeerListConnectRange(size_t range);
  void setPeerListGetTryCount(size_t count);

private:
  std::chrono::nanoseconds timedSyncInterval;
  std::chrono::nanoseconds handshakeTimeout;
  std::chrono::nanoseconds connectInterval;
  std::chrono::nanoseconds connectTimeout;
  boost::uuids::uuid networkId;
  size_t expectedOutgoingConnectionsCount;
  size_t whiteListConnectionsPercent;
  size_t peerListConnectRange;
  size_t peerListGetTryCount;
};

}
