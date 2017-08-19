// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <System/Dispatcher.h>
#include <System/ContextGroup.h>
#include <System/Event.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <System/TcpConnection.h>
#include <System/TcpConnector.h>
#include <System/TcpListener.h>
#include <System/Timer.h>
#include <gtest/gtest.h>

using namespace System;

class TcpConnectorTests : public testing::Test {
public:
  TcpConnectorTests() : event(dispatcher), listener(dispatcher, Ipv4Address("127.0.0.1"), 6666), contextGroup(dispatcher) {
  }
  
  Dispatcher dispatcher;
  Event event;
  TcpListener listener;
  ContextGroup contextGroup;
};

TEST_F(TcpConnectorTests, tcpConnector1) {
  contextGroup.spawn([&]() {
    listener.accept();
    event.set();
  });

  TcpConnector connector(dispatcher);
  contextGroup.spawn([&] { 
    connector.connect(Ipv4Address("127.0.0.1"), 6666); 
  });
  event.wait();
  dispatcher.yield();
}

TEST_F(TcpConnectorTests, tcpConnectorInterruptAfterStart) {
  contextGroup.spawn([&] { 
    ASSERT_THROW(TcpConnector(dispatcher).connect(Ipv4Address("127.0.0.1"), 6666), InterruptedException); 
  });
  contextGroup.interrupt();
}

TEST_F(TcpConnectorTests, tcpConnectorInterrupt) {
  TcpConnector connector(dispatcher);
  contextGroup.spawn([&]() {
    Timer(dispatcher).sleep(std::chrono::milliseconds(10));
    contextGroup.interrupt();
    event.set();
  });

  contextGroup.spawn([&] { 
    ASSERT_THROW(connector.connect(Ipv4Address("10.255.255.1"), 6666), InterruptedException); 
  });
  contextGroup.wait();
}

TEST_F(TcpConnectorTests, tcpConnectorUseAfterInterrupt) {
  TcpConnector connector(dispatcher);
  contextGroup.spawn([&]() {
    Timer(dispatcher).sleep(std::chrono::milliseconds(10));
    contextGroup.interrupt();
    event.set();
  });

  contextGroup.spawn([&] { 
    ASSERT_THROW(connector.connect(Ipv4Address("10.255.255.1"), 6666), InterruptedException); 
  });
  contextGroup.wait();
  contextGroup.spawn([&] { 
    ASSERT_NO_THROW(connector.connect(Ipv4Address("127.0.0.1"), 6666)); 
  });
  contextGroup.wait();
}

TEST_F(TcpConnectorTests, bindToTheSameAddressFails) {
  ASSERT_THROW(TcpListener listener2(dispatcher, Ipv4Address("127.0.0.1"), 6666), std::runtime_error);
}
