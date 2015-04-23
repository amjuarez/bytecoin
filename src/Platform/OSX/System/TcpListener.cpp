// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "TcpListener.h"
#include <cassert>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "InterruptedException.h"
#include "Dispatcher.h"
#include "TcpConnection.h"

using namespace System;

namespace {

struct ListenerContext : public Dispatcher::ContextExt {
  bool interrupted;
};

}

TcpListener::TcpListener() : dispatcher(nullptr) {
}

TcpListener::TcpListener(TcpListener&& other) : dispatcher(other.dispatcher) {
  if (other.dispatcher != nullptr) {
    listener = other.listener;
    stopped = other.stopped;
    context = other.context;
    other.dispatcher = nullptr;
  }
}

TcpListener::~TcpListener() {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    if (close(listener) == -1) {
      std::cerr << "close() failed, errno=" << errno << '.' << std::endl;
    }
  }
}

TcpListener& TcpListener::operator=(TcpListener&& other) {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    if (close(listener) == -1) {
      std::cerr << "close() failed, errno=" << errno << '.' << std::endl;
      throw std::runtime_error("TcpListener::operator=");
    }
  }

  dispatcher = other.dispatcher;
  if (other.dispatcher != nullptr) {
    listener = other.listener;
    stopped = other.stopped;
    context = other.context;
    other.dispatcher = nullptr;
  }

  return *this;
}

void TcpListener::start() {
  assert(dispatcher != nullptr);
  assert(stopped);
  stopped = false;
}

TcpListener::TcpListener(Dispatcher& dispatcher, const std::string& address, uint16_t port) : dispatcher(&dispatcher) {
  listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == -1) {
    std::cerr << "socket failed, errno=" << errno << std::endl;
  } else {
    int flags = fcntl(listener, F_GETFL, 0);
    if (flags == -1 || (fcntl(listener, F_SETFL, flags | O_NONBLOCK) == -1)) {
      std::cerr << "fcntl() failed errno=" << errno << std::endl;
    } else {
      sockaddr_in address;
      address.sin_family = AF_INET;
      address.sin_port = htons(port);
      address.sin_addr.s_addr = INADDR_ANY;
      if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) {
        std::cerr << "bind failed, errno=" << errno << std::endl;
      } else if (listen(listener, SOMAXCONN) != 0) {
        std::cerr << "listen failed, errno=" << errno << std::endl;
      } else {
        struct kevent event;
        EV_SET(&event, listener, EVFILT_READ, EV_ADD | EV_DISABLE, 0, SOMAXCONN, NULL);

        if (kevent(dispatcher.getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
          std::cerr << "kevent() failed, errno=" << errno << '.' << std::endl;
        } else {
          stopped = false;
          context = nullptr;
          return;
        }
      }
    }

    if (close(listener) == -1) {
      std::cerr << "close failed, errno=" << errno << std::endl;
    }
  }

  throw std::runtime_error("TcpListener::TcpListener");
}

TcpConnection TcpListener::accept() {
  assert(dispatcher != nullptr);
  assert(context == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  ListenerContext context2;
  context2.context = dispatcher->getCurrentContext();
  context2.interrupted = false;

  struct kevent event;
  EV_SET(&event, listener, EVFILT_READ, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, SOMAXCONN, &context2);
  if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
    std::cerr << "kevent() failed, errno=" << errno << '.' << std::endl;
  } else {
    context = &context2;
    dispatcher->yield();
    assert(dispatcher != nullptr);
    assert(context2.context == dispatcher->getCurrentContext());
    assert(context == &context2);
    context = nullptr;
    context2.context = nullptr;
    if (context2.interrupted) {
      if (close(listener) == -1) {
        std::cerr << "close failed, errno=" << errno << std::endl;
      }
      throw InterruptedException();
    }
    struct kevent event;
    EV_SET(&event, listener, EVFILT_READ, EV_ADD | EV_DISABLE, 0, 0, NULL);

    if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
      std::cerr << "kevent() failed, errno=" << errno << '.' << std::endl;
    } else {
      sockaddr inAddr;
      socklen_t inLen = sizeof(inAddr);
      int connection = ::accept(listener, &inAddr, &inLen);
      if (connection == -1) {
        std::cerr << "accept() failed, errno=" << errno << '.' << std::endl;
      } else {
        int flags = fcntl(connection, F_GETFL, 0);
        if (flags == -1 || (fcntl(connection, F_SETFL, flags | O_NONBLOCK) == -1)) {
          std::cerr << "fcntl() failed errno=" << errno << std::endl;
        } else {
          return TcpConnection(*dispatcher, connection);
        }
      }
    }
  }

  throw std::runtime_error("TcpListener::accept");
}

void TcpListener::stop() {
  assert(dispatcher != nullptr);
  assert(!stopped);
  if (context != nullptr) {
    ListenerContext* context2 = static_cast<ListenerContext*>(context);
    if (!context2->interrupted) {
      context2->interrupted = true;

      struct kevent event;
      EV_SET(&event, listener, EVFILT_READ, EV_DELETE | EV_DISABLE, 0, 0, NULL);

      if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
        std::cerr << "kevent() failed, errno=" << errno << '.' << std::endl;
        throw std::runtime_error("TcpListener::stop");
      }

      dispatcher->pushContext(context2->context);
    }
  }

  stopped = true;
}
