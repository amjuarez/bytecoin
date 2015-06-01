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

#include <cstdint>
#include <functional>
#include <map>
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

  // Platform-specific
  void addTimer(uint64_t time, void* context);
  void* getCompletionPort() const;
  void interruptTimer(uint64_t time, void* context);

private:
  void* completionPort;
  std::size_t contextCount;
  uint8_t criticalSection[2 * sizeof(long) + 4 * sizeof(void*)];
  std::queue<void*> resumingContexts;
  bool remoteNotificationSent;
  std::queue<std::function<void()>> remoteSpawningProcedures;
  uint8_t remoteSpawnOverlapped[4 * sizeof(void*)];
  std::stack<void*> reusableContexts;
  std::queue<std::function<void()>> spawningProcedures;
  void* threadHandle;
  uint32_t threadId;
  std::multimap<uint64_t, void*> timers;

  void contextProcedure();
  static void __stdcall contextProcedureStatic(void* context);
};

}
