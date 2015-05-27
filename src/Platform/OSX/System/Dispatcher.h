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

#pragma once

#include <atomic>
#include <functional>
#include <queue>
#include <stack>

namespace System {

class Dispatcher {
public:
  Dispatcher();
  Dispatcher(const Dispatcher&) = delete;
  ~Dispatcher();
  Dispatcher& operator=(const Dispatcher&) = delete;
  void clear();
  void dispatch();
  void* getCurrentContext() const;
  void pushContext(void* context);
  void remoteSpawn(std::function<void()>&& procedure);
  void spawn(std::function<void()>&& procedure);
  void yield();

  struct OperationContext {
    void *context;
    bool interrupted;
  };

  int getKqueue() const;
  int getTimer();
  void pushTimer(int timer);

#ifdef __LP64__
  static const int SIZEOF_PTHREAD_MUTEX_T = 56 + sizeof(long);
#else
  static const int SIZEOF_PTHREAD_MUTEX_T = 40 + sizeof(long);
#endif

private:
  std::stack<uint8_t*> allocatedStacks;
  std::size_t contextCount;
  void* currentContext;
  int kqueue;
  int lastCreatedTimer;
  uint8_t mutex[SIZEOF_PTHREAD_MUTEX_T];
  std::atomic<bool> remoteSpawned;
  std::queue<std::function<void()>> remoteSpawningProcedures;
  std::queue<void*> resumingContexts;
  std::queue<std::function<void()>> spawningProcedures;
  std::stack<void*> reusableContexts;
  std::stack<int> timers;

  void contextProcedure();
  static void contextProcedureStatic(intptr_t context);
};

}
