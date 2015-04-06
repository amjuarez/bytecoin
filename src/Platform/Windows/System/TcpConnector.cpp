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
#include <cassert>
#include <iostream>
#include <random>
#include <sstream>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include "InterruptedException.h"
#include "Dispatcher.h"
#include "TcpConnection.h"

using namespace System;

namespace {

struct Context : public OVERLAPPED {
  void* context;
  std::size_t connection;
  bool interrupted;
};

LPFN_CONNECTEX connectEx = nullptr;

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

void TcpConnector::stop() {
  assert(dispatcher != nullptr);
  assert(!stopped);
  if (context != nullptr) {
    Context* context2 = static_cast<Context*>(context);
    if (!context2->interrupted) {
      if (CancelIoEx(reinterpret_cast<HANDLE>(context2->connection), context2) != TRUE) {
        std::cerr << "CancelIoEx failed, result=" << GetLastError() << '.' << std::endl;
        throw std::runtime_error("TcpConnector::stop");
      }

      context2->interrupted = true;
    }
  }

  stopped = true;
}

TcpConnection TcpConnector::connect() {
  assert(dispatcher != nullptr);
  assert(context == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  std::ostringstream portStream;
  portStream << port;
  addrinfo hints = {0, AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL};
  addrinfo* addressInfos;
  int result = getaddrinfo(address.c_str(), portStream.str().c_str(), &hints, &addressInfos);
  if (result != 0) {
    std::cerr << "getaddrinfo failed, result=" << result << '.' << std::endl;
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
    SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection == INVALID_SOCKET) {
      std::cerr << "socket failed, result=" << WSAGetLastError() << '.' << std::endl;
    } else {
      sockaddr_in bindAddress;
      bindAddress.sin_family = AF_INET;
      bindAddress.sin_port = 0;
      bindAddress.sin_addr.s_addr = INADDR_ANY;
      if (bind(connection, reinterpret_cast<sockaddr*>(&bindAddress), sizeof bindAddress) != 0) {
        std::cerr << "bind failed, result=" << WSAGetLastError() << '.' << std::endl;
      } else {
        GUID guidConnectEx = WSAID_CONNECTEX;
        DWORD read = sizeof connectEx;
        if (connectEx == nullptr && WSAIoctl(connection, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidConnectEx, sizeof guidConnectEx, &connectEx, sizeof connectEx, &read, NULL, NULL) != 0) {
          std::cerr << "WSAIoctl failed, result=" << WSAGetLastError() << '.' << std::endl;
        } else {
          assert(read == sizeof connectEx);
          if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(connection), dispatcher->getCompletionPort(), 0, 0) != dispatcher->getCompletionPort()) {
            std::cerr << "CreateIoCompletionPort failed, result=" << GetLastError() << '.' << std::endl;
          } else {
            addressData.sin_port = htons(port);
            Context context2;
            context2.hEvent = NULL;
            if (connectEx(connection, reinterpret_cast<sockaddr*>(&addressData), sizeof addressData, NULL, 0, NULL, &context2) == TRUE) {
              std::cerr << "ConnectEx returned immediately, which is not supported." << std::endl;
            } else {
              int lastError = WSAGetLastError();
              if (lastError != WSA_IO_PENDING) {
                std::cerr << "ConnectEx failed, result=" << lastError << '.' << std::endl;
              } else {
                context2.context = GetCurrentFiber();
                context2.connection = connection;
                context2.interrupted = false;
                context = &context2;
                dispatcher->yield();
                assert(dispatcher != nullptr);
                assert(context2.context == GetCurrentFiber());
                assert(context2.connection == connection);
                assert(context == &context2);
                context = nullptr;
                DWORD transferred;
                DWORD flags;
                if (WSAGetOverlappedResult(connection, &context2, &transferred, FALSE, &flags) != TRUE) {
                  lastError = WSAGetLastError();
                  if (lastError == ERROR_OPERATION_ABORTED) {
                    assert(context2.interrupted);
                    if (closesocket(connection) != 0) {
                      std::cerr << "closesocket failed, result=" << WSAGetLastError() << '.' << std::endl;
                    }

                    throw InterruptedException();
                  }

                  std::cerr << "ConnectEx failed, result=" << lastError << '.' << std::endl;
                } else {
                  assert(transferred == 0);
                  assert(flags == 0);
                  return TcpConnection(*dispatcher, connection);
                }
              }
            }
          }
        }
      }

      if (closesocket(connection) != 0) {
        std::cerr << "closesocket failed, result=" << WSAGetLastError() << '.' << std::endl;
      }
    }
  }

  throw std::runtime_error("TcpConnector::connect");
}
