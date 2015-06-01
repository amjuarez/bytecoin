// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <mswsock.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include "Dispatcher.h"
#include "TcpConnection.h"

namespace System {

namespace {

struct TcpListenerContext : public OVERLAPPED {
  void* context;
  bool interrupted;
};

LPFN_ACCEPTEX acceptEx = nullptr;

}

TcpListener::TcpListener() : dispatcher(nullptr) {
}

TcpListener::TcpListener(Dispatcher& dispatcher, const Ipv4Address& address, uint16_t port) : dispatcher(&dispatcher) {
  std::string message;
  listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET) {
    message = "socket failed, result=" + std::to_string(WSAGetLastError());
  } else {
    sockaddr_in addressData;
    addressData.sin_family = AF_INET;
    addressData.sin_port = htons(port);
    addressData.sin_addr.S_un.S_addr = htonl(address.getValue());
    if (bind(listener, reinterpret_cast<sockaddr*>(&addressData), sizeof(addressData)) != 0) {
      message = "bind failed, result=" + std::to_string(WSAGetLastError());
    } else if (listen(listener, SOMAXCONN) != 0) {
      message = "listen failed, result=" + std::to_string(WSAGetLastError());
    } else {
      GUID guidAcceptEx = WSAID_ACCEPTEX;
      DWORD read = sizeof acceptEx;
      if (acceptEx == nullptr && WSAIoctl(listener, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx, sizeof guidAcceptEx, &acceptEx, sizeof acceptEx, &read, NULL, NULL) != 0) {
        message = "WSAIoctl failed, result=" + std::to_string(WSAGetLastError());
      } else {
        assert(read == sizeof acceptEx);
        if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(listener), dispatcher.getCompletionPort(), 0, 0) != dispatcher.getCompletionPort()) {
          message = "CreateIoCompletionPort failed, result=" + std::to_string(GetLastError());
        } else {
          stopped = false;
          context = nullptr;
          return;
        }
      }
    }

    int result = closesocket(listener);
    assert(result == 0);
  }

  throw std::runtime_error("TcpListener::TcpListener, " + message);
}

TcpListener::TcpListener(TcpListener&& other) : dispatcher(other.dispatcher) {
  if (dispatcher != nullptr) {
    assert(other.context == nullptr);
    listener = other.listener;
    stopped = other.stopped;
    context = nullptr;
    other.dispatcher = nullptr;
  }
}

TcpListener::~TcpListener() {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    int result = closesocket(listener);
    assert(result == 0);
  }
}

TcpListener& TcpListener::operator=(TcpListener&& other) {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    if (closesocket(listener) != 0) {
      throw std::runtime_error("TcpListener::operator=, closesocket failed, result=" + std::to_string(WSAGetLastError()));
    }
  }

  dispatcher = other.dispatcher;
  if (dispatcher != nullptr) {
    assert(other.context == nullptr);
    listener = other.listener;
    stopped = other.stopped;
    context = nullptr;
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
    TcpListenerContext* context2 = static_cast<TcpListenerContext*>(context);
    if (!context2->interrupted) {
      if (CancelIoEx(reinterpret_cast<HANDLE>(listener), context2) != TRUE) {
        DWORD lastError = GetLastError();
        if (lastError != ERROR_NOT_FOUND) {
          throw std::runtime_error("TcpListener::stop, CancelIoEx failed, result=" + std::to_string(GetLastError()));
        }
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

  std::string message;
  SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (connection == INVALID_SOCKET) {
    message = "socket failed, result=" + std::to_string(WSAGetLastError());
  } else {
    uint8_t addresses[sizeof sockaddr_in * 2 + 32];
    DWORD received;
    TcpListenerContext context2;
    context2.hEvent = NULL;
    if (acceptEx(listener, connection, addresses, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &received, &context2) == TRUE) {
      message = "AcceptEx returned immediately, which is not supported.";
    } else {
      int lastError = WSAGetLastError();
      if (lastError != WSA_IO_PENDING) {
        message = "AcceptEx failed, result=" + std::to_string(lastError);
      } else {
        context2.context = GetCurrentFiber();
        context2.interrupted = false;
        context = &context2;
        dispatcher->dispatch();
        assert(context2.context == GetCurrentFiber());
        assert(dispatcher != nullptr);
        assert(context == &context2);
        context = nullptr;
        DWORD transferred;
        DWORD flags;
        if (WSAGetOverlappedResult(listener, &context2, &transferred, FALSE, &flags) != TRUE) {
          lastError = WSAGetLastError();
          if (lastError != ERROR_OPERATION_ABORTED) {
            message = "AcceptEx failed, result=" + std::to_string(lastError);
          } else {
            assert(context2.interrupted);
            if (closesocket(connection) != 0) {
              throw std::runtime_error("TcpListener::accept, closesocket failed, result=" + std::to_string(WSAGetLastError()));
            } else {
              throw InterruptedException();
            }
          }
        } else {
          assert(transferred == 0);
          assert(flags == 0);
          if (setsockopt(connection, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<char*>(&listener), sizeof listener) != 0) {
            message = "setsockopt failed, result=" + std::to_string(WSAGetLastError());
          } else {
            if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(connection), dispatcher->getCompletionPort(), 0, 0) != dispatcher->getCompletionPort()) {
              message = "CreateIoCompletionPort failed, result=" + std::to_string(GetLastError());
            } else {
              return TcpConnection(*dispatcher, connection);
            }
          }
        }
      }
    }

    int result = closesocket(connection);
    assert(result == 0);
  }

  throw std::runtime_error("TcpListener::accept, " + message);
}

}
