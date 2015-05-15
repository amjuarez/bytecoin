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
  void spawn(std::function<void()>&& procedure);
  void yield();
  void clear();

private:
  friend class Event;
  friend class DispatcherAccessor;
  friend class TcpConnection;
  friend class TcpConnector;
  friend class TcpListener;
  friend class Timer;

  void* completionPort;
  std::size_t contextCount;
  std::queue<void*> resumingContexts;
  std::stack<void*> reusableContexts;
  std::queue<std::function<void()>> spawningProcedures;
  std::stack<void*> timers;

  void* getCompletionPort() const;
  void* getTimer();
  void pushTimer(void* timer);
  void pushContext(void* context);

  void contextProcedure();
  static void __stdcall contextProcedureStatic(void* context);
};

}
