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

#include "Dispatcher.h"
#include <cassert>
#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>

namespace System {

namespace {

struct DispatcherContext : public OVERLAPPED {
  void* context;
};

}

Dispatcher::Dispatcher() {
  static_assert(sizeof(CRITICAL_SECTION) == sizeof(Dispatcher::criticalSection), "CRITICAL_SECTION size doesn't fit sizeof(Dispatcher::criticalSection)");
  BOOL result = InitializeCriticalSectionAndSpinCount(reinterpret_cast<LPCRITICAL_SECTION>(criticalSection), 4000);
  assert(result != FALSE);
  std::string message;
  if (ConvertThreadToFiberEx(NULL, 0) == NULL) {
    message = "ConvertThreadToFiberEx failed, result=" + std::to_string(GetLastError());
  } else {
    threadHandle = OpenThread(THREAD_SET_CONTEXT, FALSE, GetCurrentThreadId());
    if (threadHandle == NULL) {
      message = "OpenThread failed, result=" + std::to_string(GetLastError());
    } else {
      completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
      if (completionPort == NULL) {
        message = "CreateIoCompletionPort failed, result=" + std::to_string(GetLastError());
      } else {
        WSADATA wsaData;
        int wsaResult = WSAStartup(0x0202, &wsaData);
        if (wsaResult != 0) {
          message = "WSAStartup failed, result=" + std::to_string(wsaResult);
        } else {
          contextCount = 0;
          remoteNotificationSent = false;
          reinterpret_cast<LPOVERLAPPED>(remoteSpawnOverlapped)->hEvent = NULL;
          threadId = GetCurrentThreadId();
          return;
        }

        BOOL result = CloseHandle(completionPort);
        assert(result == TRUE);
      }

      BOOL result = CloseHandle(threadHandle);
      assert(result == TRUE);
    }

    BOOL result = ConvertFiberToThread();
    assert(result == TRUE);
  }
  
  DeleteCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(criticalSection));
  throw std::runtime_error("Dispatcher::Dispatcher, " + message);
}

Dispatcher::~Dispatcher() {
  assert(resumingContexts.empty());
  assert(reusableContexts.size() == contextCount);
  assert(spawningProcedures.empty());
  assert(GetCurrentThreadId() == threadId);
  while (!reusableContexts.empty()) {
    DeleteFiber(reusableContexts.top());
    reusableContexts.pop();
  }

  int wsaResult = WSACleanup();
  assert(wsaResult == 0);
  BOOL result = CloseHandle(completionPort);
  assert(result == TRUE);
  result = CloseHandle(threadHandle);
  assert(result == TRUE);
  result = ConvertFiberToThread();
  assert(result == TRUE);
  DeleteCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(criticalSection));
}

void Dispatcher::clear() {
  assert(GetCurrentThreadId() == threadId);
  while (!reusableContexts.empty()) {
    DeleteFiber(reusableContexts.top());
    --contextCount;
    reusableContexts.pop();
  }
}

void Dispatcher::dispatch() {
  assert(GetCurrentThreadId() == threadId);
  void* context;
  for (;;) {
    if (!resumingContexts.empty()) {
      context = resumingContexts.front();
      resumingContexts.pop();
      break;
    }

    LARGE_INTEGER frequency;
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    QueryPerformanceFrequency(&frequency);
    uint64_t currentTime = ticks.QuadPart / (frequency.QuadPart / 1000);
    auto timerContextPair = timers.begin();
    auto end = timers.end();
    while (timerContextPair != end && timerContextPair->first <= currentTime) {
      resumingContexts.push(timerContextPair->second);
      timerContextPair = timers.erase(timerContextPair);
    }

    if (!resumingContexts.empty()) {
      context = resumingContexts.front();
      resumingContexts.pop();
      break;
    }
    
    DWORD timeout = timers.empty() ? INFINITE : static_cast<DWORD>(std::min(timers.begin()->first - currentTime, static_cast<uint64_t>(INFINITE - 1)));
    OVERLAPPED_ENTRY entry;
    ULONG actual = 0;
    if (GetQueuedCompletionStatusEx(completionPort, &entry, 1, &actual, timeout, TRUE) == TRUE) {
      if (entry.lpOverlapped == reinterpret_cast<LPOVERLAPPED>(remoteSpawnOverlapped)) {
        EnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(criticalSection));
        assert(remoteNotificationSent);
        assert(!remoteSpawningProcedures.empty());
        do {
          spawn(std::move(remoteSpawningProcedures.front()));
          remoteSpawningProcedures.pop();
        } while (!remoteSpawningProcedures.empty());

        remoteNotificationSent = false;
        LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(criticalSection));
        continue;
      }

      context = reinterpret_cast<DispatcherContext*>(entry.lpOverlapped)->context;
      break;
    }

    DWORD lastError = GetLastError();
    if (lastError == WAIT_TIMEOUT) {
      continue;
    }

    if (lastError != WAIT_IO_COMPLETION) {
      throw std::runtime_error("Dispatcher::dispatch, GetQueuedCompletionStatusEx failed, result=" + std::to_string(lastError));
    }
  }

  if (context != GetCurrentFiber()) {
    SwitchToFiber(context);
  }
}

void* Dispatcher::getCurrentContext() const {
  assert(GetCurrentThreadId() == threadId);
  return GetCurrentFiber();
}

void Dispatcher::pushContext(void* context) {
  assert(GetCurrentThreadId() == threadId);
  resumingContexts.push(context);
}

void Dispatcher::remoteSpawn(std::function<void()>&& procedure) {
  EnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(criticalSection));
  remoteSpawningProcedures.push(std::move(procedure));
  if (!remoteNotificationSent) {
    remoteNotificationSent = true;
    if (PostQueuedCompletionStatus(completionPort, 0, 0, reinterpret_cast<LPOVERLAPPED>(remoteSpawnOverlapped)) == NULL) {
      LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(criticalSection));
      throw std::runtime_error("Dispatcher::remoteSpawn, PostQueuedCompletionStatus failed, result=" + std::to_string(GetLastError()));
    };
  }

  LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(criticalSection));
}

void Dispatcher::spawn(std::function<void()>&& procedure) {
  assert(GetCurrentThreadId() == threadId);
  void* context;
  if (reusableContexts.empty()) {
    context = CreateFiberEx(16384, 131072, 0, contextProcedureStatic, this);
    if (context == NULL) {
      throw std::runtime_error("Dispatcher::spawn, CreateFiberEx failed, result=" + std::to_string(GetLastError()));
    }

    ++contextCount;
  } else {
    context = reusableContexts.top();
    reusableContexts.pop();
  }

  resumingContexts.push(context);
  spawningProcedures.emplace(std::move(procedure));
}

void Dispatcher::yield() {
  assert(GetCurrentThreadId() == threadId);
  for (;;) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    QueryPerformanceFrequency(&frequency);
    uint64_t currentTime = ticks.QuadPart / (frequency.QuadPart / 1000);
    auto timerContextPair = timers.begin();
    auto end = timers.end();
    while (timerContextPair != end && timerContextPair->first <= currentTime) {
      resumingContexts.push(timerContextPair->second);
      timerContextPair = timers.erase(timerContextPair);
    }

    OVERLAPPED_ENTRY entries[16];
    ULONG actual = 0;
    if (GetQueuedCompletionStatusEx(completionPort, entries, 16, &actual, 0, TRUE) == TRUE) {
      assert(actual > 0);
      for (ULONG i = 0; i < actual; ++i) {
        if (entries[i].lpOverlapped == reinterpret_cast<LPOVERLAPPED>(remoteSpawnOverlapped)) {
          EnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(criticalSection));
          assert(remoteNotificationSent);
          assert(!remoteSpawningProcedures.empty());
          do {
            spawn(std::move(remoteSpawningProcedures.front()));
            remoteSpawningProcedures.pop();
          } while (!remoteSpawningProcedures.empty());

          remoteNotificationSent = false;
          LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(criticalSection));
          continue;
        }

        void* context = reinterpret_cast<DispatcherContext*>(entries[i].lpOverlapped)->context;
        resumingContexts.push(context);
      }
    } else {
      DWORD lastError = GetLastError();
      if (lastError == WAIT_TIMEOUT) {
        break;
      } else if (lastError != WAIT_IO_COMPLETION) {
        throw std::runtime_error("Dispatcher::dispatch, GetQueuedCompletionStatusEx failed, result=" + std::to_string(lastError));
      }
    }
  }

  if (!resumingContexts.empty()) {
    resumingContexts.push(GetCurrentFiber());
    dispatch();
  }
}

void Dispatcher::addTimer(uint64_t time, void* context) {
  assert(GetCurrentThreadId() == threadId);
  timers.insert(std::make_pair(time, context));
}

void* Dispatcher::getCompletionPort() const {
  return completionPort;
}

void Dispatcher::interruptTimer(uint64_t time, void* context) {
  assert(GetCurrentThreadId() == threadId);
  auto range = timers.equal_range(time);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == context) {
      resumingContexts.push(context);
      timers.erase(it);
      break;
    }
  }
}

void Dispatcher::contextProcedure() {
  assert(GetCurrentThreadId() == threadId);
  for (;;) {
    assert(!spawningProcedures.empty());
    std::function<void()> procedure = std::move(spawningProcedures.front());
    spawningProcedures.pop();
    procedure();
    reusableContexts.push(GetCurrentFiber());
    dispatch();
  }
}

void __stdcall Dispatcher::contextProcedureStatic(void* context) {
  static_cast<Dispatcher*>(context)->contextProcedure();
}

}
