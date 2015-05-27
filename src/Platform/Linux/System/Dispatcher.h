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
    uint32_t events;
  };

  struct ContextPair {
    OperationContext *readContext;
    OperationContext *writeContext;
  };

  // system-dependent
  int getEpoll() const;
  int getTimer();
  void pushTimer(int timer);

#ifdef __x86_64__
# if __WORDSIZE == 64
  static const int SIZEOF_PTHREAD_MUTEX_T = 40;
# else
  static const int SIZEOF_PTHREAD_MUTEX_T = 32
# endif
#else
  static const int SIZEOF_PTHREAD_MUTEX_T = 24
#endif

private:
  std::stack<uint8_t *> allocatedStacks;
  std::size_t contextCount;
  void* currentContext;
  int epoll;
  ContextPair eventContext;
  uint8_t mutex[SIZEOF_PTHREAD_MUTEX_T];
  int remoteSpawnEvent;
  std::queue<std::function<void()>> remoteSpawningProcedures;
  std::queue<void*> resumingContexts;
  std::stack<void*> reusableContexts;
  std::queue<std::function<void()>> spawningProcedures;
  std::stack<int> timers;

  void contextProcedure();
  static void contextProcedureStatic(void* context);
};

}
