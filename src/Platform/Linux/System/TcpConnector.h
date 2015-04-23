// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <string>
#include <stdint.h>

namespace System {

class Dispatcher;
class TcpConnection;

class TcpConnector {
public:
  TcpConnector();
  TcpConnector(Dispatcher& dispatcher, const std::string& address, uint16_t port);
  TcpConnector(const TcpConnector&) = delete;
  TcpConnector(TcpConnector&& other);
  ~TcpConnector();
  TcpConnector& operator=(const TcpConnector&) = delete;
  TcpConnector& operator=(TcpConnector&& other);
  void start();
  void stop();
  TcpConnection connect();

private:
  Dispatcher* dispatcher;
  std::string address;
  uint16_t port;
  bool stopped;
  void* context;
};

}
