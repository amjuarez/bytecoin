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

#include "TcpListener.h"
#include <boost/asio/ip/tcp.hpp>
#include "System.h"
#include "TcpConnection.h"

TcpListener::TcpListener() : system(nullptr) {
}

TcpListener::TcpListener(System& system, const std::string& address, uint16_t port) : system(&system), stopped(false) {
  listener = new boost::asio::ip::tcp::acceptor(*static_cast<boost::asio::io_service*>(system.getIoService()), boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(address), port), true);
}

TcpListener::TcpListener(TcpListener&& other) : system(other.system) {
  if (other.system != nullptr) {
    listener = other.listener;
    stopped = other.stopped;
    other.system = nullptr;
  }
}

TcpListener::~TcpListener() {
  if (system != nullptr) {
    delete static_cast<boost::asio::ip::tcp::acceptor*>(listener);
  }
}

TcpListener& TcpListener::operator=(TcpListener&& other) {
  if (system != nullptr) {
    delete static_cast<boost::asio::ip::tcp::acceptor*>(listener);
  }

  system = other.system;
  if (other.system != nullptr) {
    listener = other.listener;
    stopped = other.stopped;
    other.system = nullptr;
  }

  return *this;
}

void TcpListener::start() {
  stopped = false;
}

void TcpListener::stop() {
  stopped = true;
}

TcpConnection TcpListener::accept() {
  assert(system != nullptr);
  if (stopped) {
    throw std::runtime_error("Stopped");
  }

  void* context = system->getCurrentContext();
  boost::asio::ip::tcp::socket* socket = new boost::asio::ip::tcp::socket(*static_cast<boost::asio::io_service*>(system->getIoService()));
  boost::system::error_code errorCode;
  static_cast<boost::asio::ip::tcp::acceptor*>(listener)->async_accept(*socket, [&](const boost::system::error_code& callbackErrorCode) {
    errorCode = callbackErrorCode;
    system->pushContext(context);
  });

  system->yield();
  if (errorCode) {
    delete socket;
    throw boost::system::system_error(errorCode);
  }

  return TcpConnection(*system, socket);
}
