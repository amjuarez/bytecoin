// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstddef>
#include <cstdint>

namespace System {

class Dispatcher;

class TcpConnection {
public:
  TcpConnection();
  TcpConnection(const TcpConnection&) = delete;
  TcpConnection(TcpConnection&& other);
  ~TcpConnection();
  TcpConnection& operator=(const TcpConnection&) = delete;
  TcpConnection& operator=(TcpConnection&& other);
  void start();
  void stop();
  std::size_t read(uint8_t* data, std::size_t size);
  void write(const uint8_t* data, std::size_t size);

private:
  friend class TcpConnector;
  friend class TcpListener;

  explicit TcpConnection(Dispatcher& dispatcher, std::size_t connection);

  Dispatcher* dispatcher;
  std::size_t connection;
  bool stopped;
  void* context;
};

}
