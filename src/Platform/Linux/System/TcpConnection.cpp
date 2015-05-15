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
#include <iostream>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdexcept>
#include <sys/socket.h>
#include "Dispatcher.h"
#include "InterruptedException.h"

using namespace System;

namespace {

struct ConnectionContext : public Dispatcher::ContextExt {
  bool interrupted;
};

}

TcpConnection::TcpConnection() : dispatcher(nullptr) {
}

TcpConnection::TcpConnection(Dispatcher& dispatcher, int socket) : dispatcher(&dispatcher), connection(socket), stopped(false), context(nullptr) {
  epoll_event connectionEvent;
  connectionEvent.data.fd = connection;
  connectionEvent.events = 0;
  connectionEvent.data.ptr = nullptr;

  if (epoll_ctl(dispatcher.getEpoll(), EPOLL_CTL_ADD, socket, &connectionEvent) == -1) {
    std::cerr << errno << std::endl;
    throw std::runtime_error("epoll_ctl() fail");
  }
}

TcpConnection::TcpConnection(TcpConnection&& other) : dispatcher(other.dispatcher) {
  if (other.dispatcher != nullptr) {
    connection = other.connection;
    stopped = other.stopped;
    context = other.context;
    other.dispatcher = nullptr;
  }
}

TcpConnection::~TcpConnection() {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    if (close(connection) == -1) {
      std::cerr << "close() failed, errno=" << errno << '.' << std::endl;
    }
  }
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    if (close(connection) == -1) {
      std::cerr << "close() failed, errno=" << errno << '.' << std::endl;
      throw std::runtime_error("TcpConnection::operator=");
    }
  }

  dispatcher = other.dispatcher;
  if (other.dispatcher != nullptr) {
    connection = other.connection;
    stopped = other.stopped;
    context = other.context;
    other.dispatcher = nullptr;
  }

  return *this;
}

void TcpConnection::start() {
  assert(dispatcher != nullptr);
  assert(stopped);
  stopped = false;
}

void TcpConnection::stop() {
  assert(dispatcher != nullptr);
  assert(!stopped);
  if (context != nullptr) {
    ConnectionContext *context2 = static_cast<ConnectionContext *>(context);
    if (!context2->interrupted) {

      epoll_event connectionEvent;
      connectionEvent.data.fd = connection;
      connectionEvent.events = 0;
      connectionEvent.data.ptr = nullptr;

      if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1) {
        std::cerr << errno << std::endl;
        throw std::runtime_error("epoll_ctl() fail");
      }

      context2->interrupted = true;

      if (context2->context != nullptr) {
        dispatcher->pushContext(context2->context);
      }

      if (context2->writeContext != nullptr) {
        dispatcher->pushContext(context2->writeContext);
      }
    }
  }

  stopped = true;
}

size_t TcpConnection::read(uint8_t* data, size_t size) {
  assert(dispatcher != nullptr);
  assert(context == nullptr || static_cast<Dispatcher::ContextExt*>(context)->context == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  ssize_t transferred = ::recv(connection, (void *)data, size, 0);
  if (transferred == -1) {
    if (errno != EAGAIN  && errno != EWOULDBLOCK) {
      std::cerr << "recv failed, result=" << errno << '.' << std::endl;
    } else {
      epoll_event connectionEvent;
      connectionEvent.data.fd = connection;

      ConnectionContext context2;
      if (context == nullptr) {
        context2.writeContext = nullptr;
        context2.interrupted = false;
        context2.context = dispatcher->getCurrentContext();
        context = &context2;
        connectionEvent.events = EPOLLIN | EPOLLONESHOT;
      } else {
        assert(static_cast<Dispatcher::ContextExt*>(context)->writeContext != nullptr);
        connectionEvent.events = EPOLLIN | EPOLLOUT | EPOLLONESHOT;
      }

      connectionEvent.data.ptr = context;
      if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1) {
        std::cerr << "epoll_ctl() failed, errno=" << errno << '.' << std::endl;
      } else {
        dispatcher->yield();
        assert(dispatcher != nullptr);
        assert(context2.context == dispatcher->getCurrentContext());
        if (static_cast<ConnectionContext*>(context)->interrupted) {
          context = nullptr;
          throw InterruptedException();
        }

        assert(static_cast<Dispatcher::ContextExt*>(context)->context == context2.context);
        if (static_cast<Dispatcher::ContextExt*>(context)->writeContext != nullptr) { //write is presented, rearm
          static_cast<Dispatcher::ContextExt*>(context)->context = nullptr;

          epoll_event connectionEvent;
          connectionEvent.data.fd = connection;
          connectionEvent.events = EPOLLOUT | EPOLLONESHOT;
          connectionEvent.data.ptr = context;

          if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1) {
            std::cerr << "epoll_ctl() failed, errno=" << errno << '.' << std::endl;
            throw std::runtime_error("TcpConnection::read");
          }
        } else {
          context = nullptr;
        }

        ssize_t transferred = ::recv(connection, (void *)data, size, 0);
        if (transferred == -1) {
          std::cerr << "recv failed, errno=" << errno << '.' << std::endl;
        } else {
          if (transferred == 0) {
            std::cerr << "recv return after yield with 0 bytes" << std::endl;

            int retval = -1;
            socklen_t retValLen = sizeof(retval);
            int s = getsockopt(connection, SOL_SOCKET, SO_ERROR, &retval, &retValLen);
            if (s == -1) {
              std::cerr << "getsockopt() failed, errno=" << errno << '.' << std::endl;
            } else {
              std::cerr << "recv getsockopt retval = " << retval << std::endl;
            }
          }

          assert(transferred <= size);
          return transferred;
        }
      }
    }

    throw std::runtime_error("TcpConnection::read");
  }

  assert(transferred <= size);
  return transferred;
}

void TcpConnection::write(const uint8_t* data, size_t size) {
  assert(dispatcher != nullptr);
  assert(context == nullptr || static_cast<Dispatcher::ContextExt*>(context)->writeContext == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  if (size == 0) {
    if (shutdown(connection, SHUT_WR) == -1) {
      std::cerr << "shutdown failed, errno=" << errno << '.' << std::endl;
      throw std::runtime_error("TcpConnection::write");
    }

    return;
  }

  ssize_t transferred = ::send(connection, (void *)data, size, 0);
  if (transferred == -1) {
    if (errno != EAGAIN  && errno != EWOULDBLOCK) {
      std::cerr << "send failed, result=" << errno << '.' << std::endl;
    } else {
      epoll_event connectionEvent;
      connectionEvent.data.fd = connection;

      ConnectionContext context2;
      if (context == nullptr) {
        context2.context = nullptr;
        context2.interrupted = false;
        context2.writeContext = dispatcher->getCurrentContext();
        context = &context2;
        connectionEvent.events = EPOLLOUT | EPOLLONESHOT;
      } else {
        assert(static_cast<Dispatcher::ContextExt*>(context)->context != nullptr);
        connectionEvent.events = EPOLLIN | EPOLLOUT | EPOLLONESHOT;
      }

      connectionEvent.data.ptr = context;
      if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1) {
        std::cerr << "epoll_ctl() failed, errno=" << errno << '.' << std::endl;
      } else {
        dispatcher->yield();
        assert(dispatcher != nullptr);
        assert(context2.writeContext == dispatcher->getCurrentContext());
        if (static_cast<ConnectionContext*>(context)->interrupted) {
          context = nullptr;
          throw InterruptedException();
        }

        assert(static_cast<Dispatcher::ContextExt*>(context)->writeContext == context2.writeContext);
        if (static_cast<Dispatcher::ContextExt*>(context)->context != nullptr) { //read is presented, rearm
          static_cast<Dispatcher::ContextExt*>(context)->writeContext = nullptr;

          epoll_event connectionEvent;
          connectionEvent.data.fd = connection;
          connectionEvent.events = EPOLLIN | EPOLLONESHOT;
          connectionEvent.data.ptr = context;

          if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1) {
            std::cerr << "epoll_ctl() failed, errno=" << errno << '.' << std::endl;
            throw std::runtime_error("TcpConnection::write");
          }
        } else {
          context = nullptr;
        }

        ssize_t transferred = ::send(connection, (void *)data, size, 0);
        if (transferred == -1) {
          std::cerr << "send failed, errno=" << errno << '.' << std::endl;
        } else {
          if (transferred == 0) {
            throw std::runtime_error("send transferred 0 bytes.");
          }

          assert(transferred == size);
          return;
        }
      }
    }

    throw std::runtime_error("TcpConnection::write");
  }
}
