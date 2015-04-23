// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
