// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "P2pContext.h"

#include <System/EventLock.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <System/OperationTimeout.h>

#include "LevinProtocol.h"

using namespace System;

namespace CryptoNote {

P2pContext::Message::Message(P2pMessage&& msg, Type messageType, uint32_t returnCode) :
  messageType(messageType), returnCode(returnCode) {
  type = msg.type;
  data = std::move(msg.data);
}

size_t P2pContext::Message::size() const {
  return data.size();
}

P2pContext::P2pContext(
  Dispatcher& dispatcher,
  TcpConnection&& conn,
  bool isIncoming,
  const NetworkAddress& remoteAddress,
  std::chrono::nanoseconds timedSyncInterval,
  const CORE_SYNC_DATA& timedSyncData)
  :
  incoming(isIncoming),
  remoteAddress(remoteAddress),
  dispatcher(dispatcher),
  contextGroup(dispatcher),
  timeStarted(Clock::now()),
  timedSyncInterval(timedSyncInterval),
  timedSyncData(timedSyncData),
  timedSyncTimer(dispatcher),
  timedSyncFinished(dispatcher),
  connection(std::move(conn)),
  writeEvent(dispatcher),
  readEvent(dispatcher) {
  writeEvent.set();
  readEvent.set();
  lastReadTime = timeStarted; // use current time
  contextGroup.spawn(std::bind(&P2pContext::timedSyncLoop, this));
}

P2pContext::~P2pContext() {
  stop();
  // wait for timedSyncLoop finish
  timedSyncFinished.wait();
  // ensure that all read/write operations completed
  readEvent.wait();
  writeEvent.wait();
}

PeerIdType P2pContext::getPeerId() const {
  return peerId;
}

uint16_t P2pContext::getPeerPort() const {
  return peerPort;
}

const NetworkAddress& P2pContext::getRemoteAddress() const {
  return remoteAddress;
}

bool P2pContext::isIncoming() const {
  return incoming;
}

void P2pContext::setPeerInfo(uint8_t protocolVersion, PeerIdType id, uint16_t port) {
  version = protocolVersion;
  peerId = id;
  if (isIncoming()) {
    peerPort = port;
  }
}

bool P2pContext::readCommand(LevinProtocol::Command& cmd) {
  if (stopped) {
    throw InterruptedException();
  }

  EventLock lk(readEvent);
  bool result = LevinProtocol(connection).readCommand(cmd);
  lastReadTime = Clock::now();
  return result;
}

void P2pContext::writeMessage(const Message& msg) {
  if (stopped) {
    throw InterruptedException();
  }

  EventLock lk(writeEvent);
  LevinProtocol proto(connection);

  switch (msg.messageType) {
  case P2pContext::Message::NOTIFY:
    proto.sendMessage(msg.type, msg.data, false);
    break;
  case P2pContext::Message::REQUEST:
    proto.sendMessage(msg.type, msg.data, true);
    break;
  case P2pContext::Message::REPLY:
    proto.sendReply(msg.type, msg.data, msg.returnCode);
    break;
  }
}

void P2pContext::start() {
  // stub for OperationTimeout class
} 

void P2pContext::stop() {
  if (!stopped) {
    stopped = true;
    contextGroup.interrupt();
  }
}

void P2pContext::timedSyncLoop() {
  // construct message
  P2pContext::Message timedSyncMessage{ 
    P2pMessage{ 
      COMMAND_TIMED_SYNC::ID, 
      LevinProtocol::encode(COMMAND_TIMED_SYNC::request{ timedSyncData })
    }, 
    P2pContext::Message::REQUEST 
  };

  while (!stopped) {
    try {
      timedSyncTimer.sleep(timedSyncInterval);

      OperationTimeout<P2pContext> timeout(dispatcher, *this, timedSyncInterval);
      writeMessage(timedSyncMessage);

      // check if we had read operation in given time interval
      if ((lastReadTime + timedSyncInterval * 2) < Clock::now()) {
        stop();
        break;
      }
    } catch (InterruptedException&) {
      // someone stopped us
    } catch (std::exception&) {
      stop(); // stop connection on write error
      break;
    }
  }

  timedSyncFinished.set();
}

P2pContext::Message makeReply(uint32_t command, const BinaryArray& data, uint32_t returnCode) {
  return P2pContext::Message(
    P2pMessage{ command, data },
    P2pContext::Message::REPLY,
    returnCode);
}

P2pContext::Message makeRequest(uint32_t command, const BinaryArray& data) {
  return P2pContext::Message(
    P2pMessage{ command, data },
    P2pContext::Message::REQUEST);
}

std::ostream& operator <<(std::ostream& s, const P2pContext& conn) {
  return s << "[" << conn.getRemoteAddress() << "]";
}

}
