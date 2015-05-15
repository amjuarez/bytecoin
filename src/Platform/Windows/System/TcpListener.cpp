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
#include <cassert>
#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <mswsock.h>
#include "InterruptedException.h"
#include "Dispatcher.h"
#include "TcpConnection.h"

using namespace System;

namespace {

struct Context : public OVERLAPPED {
  void* context;
  bool interrupted;
};

LPFN_ACCEPTEX acceptEx = nullptr;
LPFN_GETACCEPTEXSOCKADDRS getAcceptExSockaddrs = nullptr;

}

TcpListener::TcpListener() : dispatcher(nullptr) {
}

TcpListener::TcpListener(Dispatcher& dispatcher, const std::string& address, uint16_t port) : dispatcher(&dispatcher) {
  listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET) {
    std::cerr << "socket failed, result=" << WSAGetLastError() << '.' << std::endl;
  } else {
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) {
      std::cerr << "bind failed, result=" << WSAGetLastError() << '.' << std::endl;
    } else if (listen(listener, SOMAXCONN) != 0) {
      std::cerr << "listen failed, result=" << WSAGetLastError() << '.' << std::endl;
    } else {
      GUID guidAcceptEx = WSAID_ACCEPTEX;
      DWORD read = sizeof acceptEx;
      if (acceptEx == nullptr && WSAIoctl(listener, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx, sizeof guidAcceptEx, &acceptEx, sizeof acceptEx, &read, NULL, NULL) != 0) {
        std::cerr << "WSAIoctl failed, result=" << WSAGetLastError() << '.' << std::endl;
      } else {
        assert(read == sizeof acceptEx);
        GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
        read = sizeof getAcceptExSockaddrs;
        if (getAcceptExSockaddrs == nullptr && WSAIoctl(listener, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidGetAcceptExSockaddrs, sizeof guidGetAcceptExSockaddrs, &getAcceptExSockaddrs, sizeof getAcceptExSockaddrs, &read, NULL, NULL) != 0) {
          std::cerr << "WSAIoctl failed, result=" << WSAGetLastError() << '.' << std::endl;
        } else {
          assert(read == sizeof getAcceptExSockaddrs);
          if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(listener), dispatcher.getCompletionPort(), 0, 0) != dispatcher.getCompletionPort()) {
            std::cerr << "CreateIoCompletionPort failed, result=" << GetLastError() << '.' << std::endl;
          } else {
            stopped = false;
            context = nullptr;
            return;
          }
        }
      }
    }

    if (closesocket(listener) != 0) {
      std::cerr << "closesocket failed, result=" << WSAGetLastError() << '.' << std::endl;
    }
  }

  throw std::runtime_error("TcpListener::TcpListener");
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
    if (closesocket(listener) != 0) {
      std::cerr << "closesocket failed, result=" << WSAGetLastError() << '.' << std::endl;
    }
  }
}

TcpListener& TcpListener::operator=(TcpListener&& other) {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    if (closesocket(listener) != 0) {
      std::cerr << "closesocket failed, result=" << WSAGetLastError() << '.' << std::endl;
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

void TcpListener::stop() {
  assert(dispatcher != nullptr);
  assert(!stopped);
  if (context != nullptr) {
    Context* context2 = static_cast<Context*>(context);
    if (!context2->interrupted) {
      if (CancelIoEx(reinterpret_cast<HANDLE>(listener), context2) != TRUE) {
        std::cerr << "CancelIoEx failed, result=" << GetLastError() << '.' << std::endl;
        throw std::runtime_error("TcpListener::stop");
      }

      context2->interrupted = true;
    }
  }

  stopped = true;
}

TcpConnection TcpListener::accept() {
  assert(dispatcher != nullptr);
  assert(context == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (connection == INVALID_SOCKET) {
    std::cerr << "socket failed, result=" << WSAGetLastError() << '.' << std::endl;
  } else {
    uint8_t addresses[sizeof sockaddr_in * 2 + 32];
    DWORD received;
    Context context2;
    context2.hEvent = NULL;
    if (acceptEx(listener, connection, addresses, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &received, &context2) == TRUE) {
      std::cerr << "AcceptEx returned immediately, which is not supported." << std::endl;
    } else {
      int lastError = WSAGetLastError();
      if (lastError != WSA_IO_PENDING) {
        std::cerr << "AcceptEx failed, result=" << lastError << '.' << std::endl;
      } else {
        context2.context = GetCurrentFiber();
        context2.interrupted = false;
        context = &context2;
        dispatcher->yield();
        assert(dispatcher != nullptr);
        assert(context2.context == GetCurrentFiber());
        assert(context == &context2);
        context = nullptr;
        DWORD transferred;
        DWORD flags;
        if (WSAGetOverlappedResult(listener, &context2, &transferred, FALSE, &flags) != TRUE) {
          lastError = WSAGetLastError();
          if (lastError == ERROR_OPERATION_ABORTED) {
            assert(context2.interrupted);
            if (closesocket(connection) != 0) {
              std::cerr << "closesocket failed, result=" << WSAGetLastError() << '.' << std::endl;
            }

            throw InterruptedException();
          }

          std::cerr << "AcceptEx failed, result=" << lastError << '.' << std::endl;
        } else {
          assert(transferred == 0);
          assert(flags == 0);
          if (setsockopt(connection, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<char*>(&listener), sizeof listener) != 0) {
            std::cerr << "setsockopt failed, result=" << WSAGetLastError() << '.' << std::endl;
          } else {
            if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(connection), dispatcher->getCompletionPort(), 0, 0) != dispatcher->getCompletionPort()) {
              std::cerr << "CreateIoCompletionPort failed, result=" << GetLastError() << '.' << std::endl;
            } else {
              //sockaddr_in* local;
              //int localSize;
              //sockaddr_in* remote;
              //int remoteSize;
              //static_cast<LPFN_GETACCEPTEXSOCKADDRS>(getAcceptExSockaddrs)(addresses, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, reinterpret_cast<sockaddr**>(&local), &localSize, reinterpret_cast<sockaddr**>(&remote), &remoteSize);
              //assert(localSize == sizeof sockaddr_in);
              //assert(remoteSize == sizeof sockaddr_in);
              //std::cout << "Client connected from " << static_cast<unsigned int>(remote->sin_addr.S_un.S_un_b.s_b1) << '.' << static_cast<unsigned int>(remote->sin_addr.S_un.S_un_b.s_b2) << '.' << static_cast<unsigned int>(remote->sin_addr.S_un.S_un_b.s_b3) << '.' << static_cast<unsigned int>(remote->sin_addr.S_un.S_un_b.s_b4) << ':' << remote->sin_port << std::endl;
              return TcpConnection(*dispatcher, connection);
            }
          }
        }
      }
    }

    if (closesocket(connection) != 0) {
      std::cerr << "closesocket failed, result=" << WSAGetLastError() << '.' << std::endl;
    }
  }

  throw std::runtime_error("TcpListener::accept");
}
