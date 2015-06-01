// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <System/TcpConnection.h>
#include <System/TcpConnector.h>
#include <System/TcpListener.h>
#include <System/Timer.h>
#include <gtest/gtest.h>

using namespace System;

TEST(TcpConnectorTest, tcpConnector1) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  TcpListener listener(dispatcher, Ipv4Address("127.0.0.1"), 6666);
  dispatcher.spawn([&]() {
    listener.accept();
    event.set();
  });

  TcpConnector connector(dispatcher);
  connector.stop();
  connector.start();
  connector.connect(Ipv4Address("127.0.0.1"), 6666);
  connector.stop();
  connector.start();
  event.wait();
  dispatcher.yield();
}

TEST(TcpConnectorTest, tcpConnector2) {
  Dispatcher dispatcher;
  TcpConnector connector(dispatcher);
  connector.stop();
  connector.start();
  connector.stop();
  ASSERT_THROW(connector.connect(Ipv4Address("127.0.0.1"), 6666), InterruptedException);
}

TEST(TcpConnectorTest, tcpConnector3) {
  Dispatcher dispatcher;
  TcpConnector connector(dispatcher);
  Event event(dispatcher);
  dispatcher.spawn([&]() {
    Timer(dispatcher).sleep(std::chrono::milliseconds(10));
    connector.stop();
    event.set();
  });

  ASSERT_THROW(connector.connect(Ipv4Address("10.255.255.1"), 6666), InterruptedException);
  event.wait();
}

TEST(TcpConnectorTest, bindToTheSameAddressFails) {
  Dispatcher dispatcher;
  TcpListener listener1(dispatcher, Ipv4Address("127.0.0.1"), 6666);
  ASSERT_THROW(TcpListener listener2(dispatcher, Ipv4Address("127.0.0.1"), 6666), std::runtime_error);
}
