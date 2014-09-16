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

#include "TcpConnector.h"
#include <boost/asio/ip/tcp.hpp>
#include "System.h"
#include "TcpConnection.h"

TcpConnector::TcpConnector() : system(nullptr) {
}

TcpConnector::TcpConnector(System& system, const std::string& address, uint16_t port) : system(&system), m_address(address), m_port(port) { }

TcpConnector::TcpConnector(TcpConnector&& other) : system(other.system) {
  if (other.system != nullptr) {
    m_address = other.m_address;
    m_port = other.m_port;
    other.system = nullptr;
  }
}

TcpConnector::~TcpConnector() {
}

TcpConnector& TcpConnector::operator=(TcpConnector&& other) {
  system = other.system;
  if (other.system != nullptr) {
    m_address = other.m_address;
    m_port = other.m_port;
    other.system = nullptr;
  }

  return *this;
}

TcpConnection TcpConnector::connect() {
  assert(system != nullptr);
  void* context = system->getCurrentContext();
  boost::asio::ip::tcp::socket* socket = new boost::asio::ip::tcp::socket(*static_cast<boost::asio::io_service*>(system->getIoService()));
  boost::system::error_code errorCode;
  socket->async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(m_address), m_port), [&](const boost::system::error_code& callbackErrorCode) {
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
