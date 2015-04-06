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

#include "Dispatcher.h"
#include <cassert>
#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>

using namespace System;

namespace {

struct OverlappedExt : public OVERLAPPED {
  void* context;
};

}

Dispatcher::Dispatcher() {
  completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (completionPort == NULL) {
    std::cerr << "CreateIoCompletionPort failed, result=" << GetLastError() << '.' << std::endl;
  } else {
    if (ConvertThreadToFiberEx(NULL, 0) == NULL) {
      std::cerr << "ConvertThreadToFiberEx failed, result=" << GetLastError() << '.' << std::endl;
    } else {
      WSADATA wsaData;
      int result = WSAStartup(0x0202, &wsaData);
      if (result != 0) {
        std::cerr << "WSAStartup failed, result=" << result << '.' << std::endl;
      } else {
        contextCount = 0;
        return;
      }

      if (ConvertFiberToThread() != TRUE) {
        std::cerr << "ConvertFiberToThread failed, result=" << GetLastError() << '.' << std::endl;
      }
    }

    if (CloseHandle(completionPort) != TRUE) {
      std::cerr << "CloseHandle failed, result=" << GetLastError() << '.' << std::endl;
    }
  }

  throw std::runtime_error("Dispatcher::Dispatcher");
}

Dispatcher::~Dispatcher() {
  assert(resumingContexts.empty());
  assert(reusableContexts.size() == contextCount);
  assert(spawningProcedures.empty());
  while (!reusableContexts.empty()) {
    DeleteFiber(reusableContexts.top());
    reusableContexts.pop();
  }

  while (!timers.empty()) {
    if (CloseHandle(timers.top()) != TRUE) {
      std::cerr << "CloseHandle failed, result=" << GetLastError() << '.' << std::endl;
    }

    timers.pop();
  }

  if (WSACleanup() != 0) {
    std::cerr << "WSACleanup failed, result=" << WSAGetLastError() << '.' << std::endl;
  }

  if (ConvertFiberToThread() != TRUE) {
    std::cerr << "ConvertFiberToThread failed, result=" << GetLastError() << '.' << std::endl;
  }

  if (CloseHandle(completionPort) != TRUE) {
    std::cerr << "CloseHandle failed, result=" << GetLastError() << '.' << std::endl;
  }
}

void* Dispatcher::getCompletionPort() const {
  return completionPort;
}

void* Dispatcher::getTimer() {
  void* timer;
  if (timers.empty()) {
    timer = CreateWaitableTimer(NULL, FALSE, NULL);
    if (timer == NULL) {
      std::cerr << "CreateWaitableTimer failed, result=" << GetLastError() << '.' << std::endl;
      throw std::runtime_error("Dispatcher::getTimer");
    }
  } else {
    timer = timers.top();
    timers.pop();
  }

  return timer;
}

void Dispatcher::pushTimer(void* timer) {
  timers.push(timer);
}

void Dispatcher::pushContext(void* context) {
  resumingContexts.push(context);
}

void Dispatcher::spawn(std::function<void()>&& procedure) {
  void* context;
  if (reusableContexts.empty()) {
    context = CreateFiberEx(16384, 65536, 0, contextProcedureStatic, this);
    if (context == NULL) {
      std::cerr << "CreateFiberEx failed, result=" << GetLastError() << '.' << std::endl;
      throw std::runtime_error("Dispatcher::spawn");
    }
    ++contextCount;
  } else {
    context = reusableContexts.top();
    reusableContexts.pop();
  }

  resumingContexts.push(context);
  spawningProcedures.emplace(std::move(procedure));
}

void Dispatcher::clear() {
//TODO
}

void Dispatcher::yield() {
  void* context;
  for (;;) {
    if (!resumingContexts.empty()) {
      context = resumingContexts.front();
      resumingContexts.pop();
      break;
    }

    OVERLAPPED_ENTRY entry;
    ULONG actual = 0;
    if (GetQueuedCompletionStatusEx(completionPort, &entry, 1, &actual, INFINITE, TRUE) == TRUE) {
      context = reinterpret_cast<OverlappedExt*>(entry.lpOverlapped)->context;
      break;
    }

    DWORD lastError = GetLastError();
    if (lastError != WAIT_IO_COMPLETION) {
      std::cerr << "GetQueuedCompletionStatusEx failed, result=" << lastError << '.' << std::endl;
      throw std::runtime_error("Dispatcher::yield");
    }
  }

  if (context != GetCurrentFiber()) {
    SwitchToFiber(context);
  }
}

void Dispatcher::contextProcedure() {
  for (;;) {
    assert(!spawningProcedures.empty());
    std::function<void()> procedure = std::move(spawningProcedures.front());
    spawningProcedures.pop();
    procedure();
    reusableContexts.push(GetCurrentFiber());
    yield();
  }
}

void __stdcall Dispatcher::contextProcedureStatic(void* context) {
  static_cast<Dispatcher*>(context)->contextProcedure();
}
