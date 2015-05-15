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
#include <cassert>
#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include "InterruptedException.h"
#include "Dispatcher.h"

using namespace System;

namespace {

struct OverlappedExt : public OVERLAPPED {
  void* context;
  bool interrupted;
};

struct Context {
  Dispatcher* dispatcher;
  OverlappedExt* read;
  OverlappedExt* write;
};

}

TcpConnection::TcpConnection() : dispatcher(nullptr) {
}

TcpConnection::TcpConnection(Dispatcher& dispatcher, std::size_t connection) : dispatcher(&dispatcher), connection(connection), stopped(false), context(nullptr) {
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
    if (closesocket(connection) != 0) {
      std::cerr << "closesocket failed, result=" << WSAGetLastError() << '.' << std::endl;
    }
  }
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    if (closesocket(connection) != 0) {
      std::cerr << "closesocket failed, result=" << WSAGetLastError() << '.' << std::endl;
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
    Context* context2 = static_cast<Context*>(context);
    if (context2->read != nullptr && !context2->read->interrupted) {
      if (CancelIoEx(reinterpret_cast<HANDLE>(connection), context2->read) != TRUE) {
        std::cerr << "CancelIoEx failed, result=" << GetLastError() << '.' << std::endl;
        throw std::runtime_error("TcpConnection::stop");
      }

      context2->read->interrupted = true;
    }

    if (context2->write != nullptr && !context2->write->interrupted) {
      if (CancelIoEx(reinterpret_cast<HANDLE>(connection), context2->write) != TRUE) {
        std::cerr << "CancelIoEx failed, result=" << GetLastError() << '.' << std::endl;
        throw std::runtime_error("TcpConnection::stop");
      }

      context2->write->interrupted = true;
    }
  }

  stopped = true;
}

size_t TcpConnection::read(uint8_t* data, size_t size) {
  assert(dispatcher != nullptr);
  assert(context == nullptr || static_cast<Context*>(context)->read == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  WSABUF buf{static_cast<ULONG>(size), reinterpret_cast<char*>(data)};
  DWORD flags = 0;
  OverlappedExt overlapped;
  overlapped.hEvent = NULL;
  if (WSARecv(connection, &buf, 1, NULL, &flags, &overlapped, NULL) != 0) {
    int lastError = WSAGetLastError();
    if (lastError != WSA_IO_PENDING) {
      std::cerr << "WSARecv failed, result=" << lastError << '.' << std::endl;
      throw std::runtime_error("TcpConnection::read");
    }
  }

  assert(flags == 0);
  Context context2;
  if (context == nullptr) {
    context2.dispatcher = dispatcher;
    context2.write = nullptr;
    context = &context2;
  }

  overlapped.context = GetCurrentFiber();
  overlapped.interrupted = false;
  static_cast<Context*>(context)->read = &overlapped;
  dispatcher->yield();
  assert(dispatcher != nullptr);
  assert(overlapped.context == GetCurrentFiber());
  assert(static_cast<Context*>(context)->read == &overlapped);
  if (static_cast<Context*>(context)->write != nullptr) {
    static_cast<Context*>(context)->read = nullptr;
  } else {
    context = nullptr;
  }

  DWORD transferred;
  if (WSAGetOverlappedResult(connection, &overlapped, &transferred, FALSE, &flags) != TRUE) {
    int lastError = WSAGetLastError();
    if (lastError == ERROR_OPERATION_ABORTED) {
      assert(overlapped.interrupted);
      throw InterruptedException();
    }

    std::cerr << "WSARecv failed, result=" << lastError << '.' << std::endl;
    throw std::runtime_error("TcpConnection::read");
  }

  assert(transferred <= size);
  assert(flags == 0);
  return transferred;
}

void TcpConnection::write(const uint8_t* data, size_t size) {
  assert(dispatcher != nullptr);
  assert(context == nullptr || static_cast<Context*>(context)->write == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  if (size == 0) {
    if (shutdown(connection, SD_SEND) != 0) {
      std::cerr << "shutdown failed, result=" << WSAGetLastError() << '.' << std::endl;
      throw std::runtime_error("TcpConnection::write");
    }

    return;
  }

  WSABUF buf{static_cast<ULONG>(size), reinterpret_cast<char*>(const_cast<uint8_t*>(data))};
  OverlappedExt overlapped;
  overlapped.hEvent = NULL;
  if (WSASend(connection, &buf, 1, NULL, 0, &overlapped, NULL) != 0) {
    int lastError = WSAGetLastError();
    if (lastError != WSA_IO_PENDING) {
      std::cerr << "WSASend failed, result=" << lastError << '.' << std::endl;
      throw std::runtime_error("TcpConnection::write");
    }
  }

  Context context2;
  if (context == nullptr) {
    context2.dispatcher = dispatcher;
    context2.read = nullptr;
    context = &context2;
  }

  overlapped.context = GetCurrentFiber();
  overlapped.interrupted = false;
  static_cast<Context*>(context)->write = &overlapped;
  dispatcher->yield();
  assert(dispatcher != nullptr);
  assert(overlapped.context == GetCurrentFiber());
  assert(static_cast<Context*>(context)->write == &overlapped);
  if (static_cast<Context*>(context)->read != nullptr) {
    static_cast<Context*>(context)->write = nullptr;
  } else {
    context = nullptr;
  }

  DWORD transferred;
  DWORD flags;
  if (WSAGetOverlappedResult(connection, &overlapped, &transferred, FALSE, &flags) != TRUE) {
    int lastError = WSAGetLastError();
    if (lastError == ERROR_OPERATION_ABORTED) {
      assert(overlapped.interrupted);
      throw InterruptedException();
    }

    std::cerr << "WSSend failed, result=" << lastError << '.' << std::endl;
    throw std::runtime_error("TcpConnection::write");
  }

  assert(transferred == size);
  assert(flags == 0);
}
