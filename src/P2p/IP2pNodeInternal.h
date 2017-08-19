// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <list>
#include "P2pProtocolDefinitions.h"

namespace CryptoNote {

class P2pContext;

class IP2pNodeInternal {
public:
  virtual const CORE_SYNC_DATA& getGenesisPayload() const = 0;
  virtual std::list<PeerlistEntry> getLocalPeerList() const = 0;
  virtual basic_node_data getNodeData() const = 0;
  virtual PeerIdType getPeerId() const = 0;

  virtual void handleNodeData(const basic_node_data& node, P2pContext& ctx) = 0;
  virtual bool handleRemotePeerList(const std::list<PeerlistEntry>& peerlist, time_t local_time) = 0;
  virtual void tryPing(P2pContext& ctx) = 0;
};

}
