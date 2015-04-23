// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "TcpConnection.h"
#include <unistd.h>
#include <assert.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/event.h>
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

TcpConnection::TcpConnection(Dispatcher& dispatcher, int socket) : dispatcher(&dispatcher), connection(socket), stopped(false), readContext(nullptr), writeContext(nullptr) {
}

TcpConnection::TcpConnection(TcpConnection&& other) : dispatcher(other.dispatcher) {
  if (other.dispatcher != nullptr) {
    connection = other.connection;
    stopped = other.stopped;
    readContext = other.readContext;
    writeContext = other.writeContext;
    other.dispatcher = nullptr;
  }
}

TcpConnection::~TcpConnection() {
  if (dispatcher != nullptr) {
    assert(readContext == nullptr);
    assert(writeContext == nullptr);
    if (close(connection) == -1) {
      std::cerr << "close() failed, errno=" << errno << '.' << std::endl;
    }
  }
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) {
  if (dispatcher != nullptr) {
    assert(readContext == nullptr);
    assert(writeContext == nullptr);
    if (close(connection) == -1) {
      std::cerr << "close() failed, errno=" << errno << '.' << std::endl;
      throw std::runtime_error("TcpConnection::operator=");
    }
  }

  dispatcher = other.dispatcher;
  if (other.dispatcher != nullptr) {
    connection = other.connection;
    stopped = other.stopped;
    readContext = other.readContext;
    writeContext = other.writeContext;
    other.dispatcher = nullptr;
  }

  return *this;
}

void TcpConnection::start() {
  assert(dispatcher != nullptr);
  assert(stopped);
  stopped = false;
}

size_t TcpConnection::read(uint8_t* data, size_t size) {
  assert(dispatcher != nullptr);
  assert(readContext == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  ssize_t transferred = ::recv(connection, (void *)data, size, 0);
  if (transferred == -1) {
    if (errno != EAGAIN  && errno != EWOULDBLOCK) {
      std::cerr << "recv failed, result=" << errno << '.' << std::endl;
    } else {
      ConnectionContext context2;
      context2.context = dispatcher->getCurrentContext();
      context2.interrupted = false;
      struct kevent event;
      EV_SET(&event, connection, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT, 0, 0, &context2);
      if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
        std::cerr << "kevent() failed, errno=" << errno << '.' << std::endl;
      } else {
        readContext = &context2;
        dispatcher->yield();
        assert(dispatcher != nullptr);
        assert(context2.context == dispatcher->getCurrentContext());
        assert(readContext == &context2);
        readContext = nullptr;
        context2.context = nullptr;
        if (context2.interrupted) {
          throw InterruptedException();
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
  assert(writeContext == nullptr);
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
      ConnectionContext context2;
      context2.context = dispatcher->getCurrentContext();
      context2.interrupted = false;
      struct kevent event;
      EV_SET(&event, connection, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, &context2);
      if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
        std::cerr << "kevent() failed, errno=" << errno << '.' << std::endl;
      } else {
        writeContext = &context2;
        dispatcher->yield();
        assert(dispatcher != nullptr);
        assert(context2.context == dispatcher->getCurrentContext());
        assert(writeContext == &context2);
        writeContext = nullptr;
        context2.context = nullptr;
        if (context2.interrupted) {
          throw InterruptedException();
        }

        ssize_t transferred = ::send(connection, (void *)data, size, 0);
        if (transferred == -1) {
          std::cerr << "recv failed, errno=" << errno << '.' << std::endl;
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

void TcpConnection::stop() {
  assert(dispatcher != nullptr);
  assert(!stopped);
  if (writeContext != nullptr && static_cast<ConnectionContext*>(writeContext)->context != nullptr) {
    ConnectionContext* context2 = static_cast<ConnectionContext*>(writeContext);
    if (!context2->interrupted) {
      struct kevent event;
      EV_SET(&event, connection, EVFILT_WRITE, EV_DELETE | EV_DISABLE, 0, 0, NULL);

      if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
        std::cerr << "kevent() failed, errno=" << errno << '.' << std::endl;
        throw std::runtime_error("TcpListener::stop");
      }

      context2->interrupted = true;
      dispatcher->pushContext(context2->context);
    }
  }

  if (readContext != nullptr && static_cast<ConnectionContext*>(readContext)->context != nullptr) {
    ConnectionContext* context2 = static_cast<ConnectionContext*>(readContext);
    if (!context2->interrupted) {
      struct kevent event;
      EV_SET(&event, connection, EVFILT_READ, EV_DELETE | EV_DISABLE, 0, 0, NULL);

      if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
        std::cerr << "kevent() failed, errno=" << errno << '.' << std::endl;
        throw std::runtime_error("TcpListener::stop");
      }

      context2->interrupted = true;
      dispatcher->pushContext(context2->context);
    }
  }

  stopped = true;
}
