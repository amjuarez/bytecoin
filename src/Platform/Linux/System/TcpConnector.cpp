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

#include <stdexcept>
#include <iostream>
#include <random>
#include <sstream>
#include <cassert>

#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "Dispatcher.h"
#include "TcpConnection.h"
#include "InterruptedException.h"

using namespace System;

namespace {

struct ConnectorContext : public Dispatcher::ContextExt {
  int connection;
  bool interrupted;
};

}

TcpConnector::TcpConnector() : dispatcher(nullptr) {
}

TcpConnector::TcpConnector(Dispatcher& dispatcher, const std::string& address, uint16_t port) : dispatcher(&dispatcher), address(address), port(port), stopped(false), context(nullptr) {
}

TcpConnector::TcpConnector(TcpConnector&& other) : dispatcher(other.dispatcher) {
  if (other.dispatcher != nullptr) {
    address = other.address;
    port = other.port;
    stopped = other.stopped;
    context = other.context;
    other.dispatcher = nullptr;
  }
}

TcpConnector::~TcpConnector() {
}

TcpConnector& TcpConnector::operator=(TcpConnector&& other) {
  dispatcher = other.dispatcher;
  if (other.dispatcher != nullptr) {
    address = other.address;
    port = other.port;
    stopped = other.stopped;
    context = other.context;
    other.dispatcher = nullptr;
  }

  return *this;
}

void TcpConnector::start() {
  assert(dispatcher != nullptr);
  assert(stopped);
  stopped = false;
}

TcpConnection TcpConnector::connect() {
  assert(dispatcher != nullptr);
  assert(context == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  std::ostringstream portStream;
  portStream << port;
  addrinfo hints = { 0, AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL };
  addrinfo *addressInfos;
  int result = getaddrinfo(address.c_str(), portStream.str().c_str(), &hints, &addressInfos);
  if (result == -1) {
    std::cerr << "getaddrinfo failed, errno=" << errno << '.' << std::endl;
  } else {
    std::size_t count = 0;
    for (addrinfo* addressInfo = addressInfos; addressInfo != nullptr; addressInfo = addressInfo->ai_next) {
      ++count;
    }

    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<std::size_t> distribution(0, count - 1);
    std::size_t index = distribution(generator);
    addrinfo* addressInfo = addressInfos;
    for (std::size_t i = 0; i < index; ++i) {
      addressInfo = addressInfo->ai_next;
    }

    sockaddr_in addressData = *reinterpret_cast<sockaddr_in*>(addressInfo->ai_addr);
    freeaddrinfo(addressInfo);
    int connection = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection == -1) {
      std::cerr << "socket failed, errno=" << errno << '.' << std::endl;
    } else {
      sockaddr_in bindAddress;
      bindAddress.sin_family = AF_INET;
      bindAddress.sin_port = 0;
      bindAddress.sin_addr.s_addr = INADDR_ANY;
      if (bind(connection, reinterpret_cast<sockaddr*>(&bindAddress), sizeof bindAddress) != 0) {
        std::cerr << "bind failed, errno=" << errno << '.' << std::endl;
      } else {
        int flags = fcntl(connection, F_GETFL, 0);
        if (flags == -1 || fcntl(connection, F_SETFL, flags | O_NONBLOCK) == -1) {
          std::cerr << "fcntl() failed errno=" << errno << std::endl;
        } else {
          int result = ::connect(connection, reinterpret_cast<sockaddr *>(&addressData), sizeof addressData);
          if (result == -1) {
            if (errno == EINPROGRESS) {
              ConnectorContext context2;
              context2.writeContext = dispatcher->getCurrentContext();
              context2.context = nullptr;
              context2.interrupted = false;
              context2.connection = connection;
              context = &context2;

              epoll_event connectEvent;
              connectEvent.data.fd = connection;
              connectEvent.events = EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLONESHOT;
              connectEvent.data.ptr = context;
              if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_ADD, connection, &connectEvent) == -1) {
                std::cerr << "epoll_ctl() failed, errno=" << errno << '.' << std::endl;
              } else {
                dispatcher->yield();
                assert(dispatcher != nullptr);
                assert(context2.writeContext == dispatcher->getCurrentContext());
                assert(context == &context2);
                context = nullptr;
                context2.writeContext = nullptr;
                if (context2.interrupted) {
                  if (close(connection) == -1) {
                    std::cerr << "close failed, errno=" << errno << std::endl;
                  }

                  throw InterruptedException();
                }

                if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_DEL, connection, NULL) == -1) {
                  std::cerr << "epoll_ctl() failed, errno=" << errno << '.' << std::endl;
                } else {
                  int retval = -1;
                  socklen_t retValLen = sizeof(retval);
                  int s = getsockopt(connection, SOL_SOCKET, SO_ERROR, &retval, &retValLen);
                  if (s == -1) {
                    std::cerr << "getsockopt() failed, errno=" << errno << '.' << std::endl;
                  } else {
                    if (retval != 0) {
                      std::cerr << "connect failed; getsockopt retval = " << retval << std::endl;
                    } else {
                      return TcpConnection(*dispatcher, connection);
                    }
                  }
                }
              }
            }
          } else {
            return TcpConnection(*dispatcher, connection);
          }
        }
      }

      if (close(connection) == -1) {
        std::cerr << "close failed, errno=" << errno << std::endl;
      }
    }
  }

  throw std::runtime_error("TcpConnector::connect");
}

void TcpConnector::stop() {
  assert(dispatcher != nullptr);
  assert(!stopped);
  if (context != nullptr) {
    ConnectorContext* context2 = static_cast<ConnectorContext*>(context);
    if (!context2->interrupted) {
      if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_DEL, context2->connection, NULL) == -1) {
        std::cerr << "epoll_ctl() failed, errno=" << errno << '.' << std::endl;
        throw std::runtime_error("TcpConnector::stop");
      }

      dispatcher->pushContext(context2->writeContext);
      context2->interrupted = true;
    }
  }

  stopped = true;
}
