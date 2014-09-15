// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
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

#include "TcpConnection.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include "System.h"

TcpConnection::TcpConnection() : system(nullptr) {
}

TcpConnection::TcpConnection(System& system, void* socket) : system(&system), socket(socket), stopped(false) {
}

TcpConnection::TcpConnection(TcpConnection&& other) : system(other.system) {
  if (other.system != nullptr) {
    socket = other.socket;
    stopped = other.stopped;
    other.system = nullptr;
  }
}

TcpConnection::~TcpConnection() {
  if (system != nullptr) {
    delete static_cast<boost::asio::ip::tcp::socket*>(socket);
  }
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) {
  if (system != nullptr) {
    delete static_cast<boost::asio::ip::tcp::socket*>(socket);
  }

  system = other.system;
  if (other.system != nullptr) {
    socket = other.socket;
    stopped = other.stopped;
    other.system = nullptr;
  }

  return *this;
}

void TcpConnection::start() {
  stopped = false;
}

void TcpConnection::stop() {
  stopped = true;
}

size_t TcpConnection::read(uint8_t* data, size_t size) {
  assert(system != nullptr);
  if (stopped) {
    throw std::runtime_error("Stopped");
  }

  void* context = system->getCurrentContext();
  boost::system::error_code errorCode;
  std::size_t transferred;
  static_cast<boost::asio::ip::tcp::socket*>(socket)->async_read_some(boost::asio::buffer(data, size), [&](const boost::system::error_code& callbackErrorCode, std::size_t callbackTransferred) {
    errorCode = callbackErrorCode;
    transferred = callbackTransferred;
    system->pushContext(context);
  });

  system->yield();
  if (errorCode) {
    throw boost::system::system_error(errorCode);
  }

  return transferred;
}

void TcpConnection::write(const uint8_t* data, size_t size) {
  assert(system != nullptr);
  if (stopped) {
    throw std::runtime_error("Stopped");
  }

  void* context = system->getCurrentContext();
  boost::system::error_code errorCode;
  std::size_t transferred;
  boost::asio::async_write(*static_cast<boost::asio::ip::tcp::socket*>(socket), boost::asio::buffer(data, size), [&](const boost::system::error_code& callbackErrorCode, std::size_t callbackTransferred) {
    errorCode = callbackErrorCode;
    transferred = callbackTransferred;
    system->pushContext(context);
  });

  system->yield();
  if (errorCode) {
    throw boost::system::system_error(errorCode);
  }

  assert(transferred == size);
}
