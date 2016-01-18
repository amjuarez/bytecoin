// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <string>

namespace System {

class Dispatcher;
class Ipv4Address;
class TcpConnection;

class TcpConnector {
public:
  TcpConnector();
  explicit TcpConnector(Dispatcher& dispatcher);
  TcpConnector(const TcpConnector&) = delete;
  TcpConnector(TcpConnector&& other);
  ~TcpConnector();
  TcpConnector& operator=(const TcpConnector&) = delete;
  TcpConnector& operator=(TcpConnector&& other);
  TcpConnection connect(const Ipv4Address& address, uint16_t port);

private:
  Dispatcher* dispatcher;
  void* context;
};

}
