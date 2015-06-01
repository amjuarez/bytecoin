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

TEST(TcpListenerTest, tcpListener1) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  TcpListener listener(dispatcher, Ipv4Address("127.0.0.1"), 6666);
  dispatcher.spawn([&]() {
    TcpConnector connector(dispatcher);
    connector.connect(Ipv4Address("127.0.0.1"), 6666);
    event.set();
  });

  listener.stop();
  listener.start();
  listener.accept();
  listener.stop();
  listener.start();
  event.wait();
}

TEST(TcpListenerTest, tcpListener2) {
  bool stopped = false;
  Dispatcher dispatcher;
  TcpListener listener(dispatcher, Ipv4Address("127.0.0.1"), 6666);
  listener.stop();
  listener.start();
  listener.stop();

  try {
    listener.accept();
  } catch (InterruptedException&) {
    stopped = true;
  }

  ASSERT_TRUE(stopped);
}

TEST(TcpListenerTest, tcpListener3) {
  bool stopped = false;
  Dispatcher dispatcher;
  Event event(dispatcher);
  TcpListener listener(dispatcher, Ipv4Address("127.0.0.1"), 6666);
  dispatcher.spawn([&]() {
    Timer(dispatcher).sleep(std::chrono::milliseconds(10));
    listener.stop();
    event.set();
  });

  try {
    listener.accept();
  } catch (InterruptedException&) {
    stopped = true;
  }

  event.wait();
  ASSERT_TRUE(stopped);
}
