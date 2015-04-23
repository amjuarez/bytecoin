// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Timer.h"
#include <cassert>
#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "InterruptedException.h"
#include "Dispatcher.h"

using namespace System;

namespace System {

class DispatcherAccessor {
public:
  DispatcherAccessor(Dispatcher* dispatcher, void* context) {
    dispatcher->pushContext(context);
  }
};

}

namespace {

struct Context {
  Dispatcher* dispatcher;
  void* context;
  bool interrupted;
};

void __stdcall callbackProcedure(void* lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue) {
  Context* context = static_cast<Context*>(lpArgToCompletionRoutine);
  assert(context->context != nullptr);
  DispatcherAccessor(context->dispatcher, context->context);
  context->context = nullptr;
}

}

Timer::Timer() : dispatcher(nullptr) {
}

Timer::Timer(Dispatcher& dispatcher) : dispatcher(&dispatcher), stopped(false), context(nullptr) {
  timer = dispatcher.getTimer();
}

Timer::Timer(Timer&& other) : dispatcher(other.dispatcher) {
  if (other.dispatcher != nullptr) {
    timer = other.timer;
    stopped = other.stopped;
    context = other.context;
    other.dispatcher = nullptr;
  }
}

Timer::~Timer() {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    dispatcher->pushTimer(timer);
  }
}

Timer& Timer::operator=(Timer&& other) {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    dispatcher->pushTimer(timer);
  }

  dispatcher = other.dispatcher;
  if (other.dispatcher != nullptr) {
    timer = other.timer;
    stopped = other.stopped;
    context = other.context;
    other.dispatcher = nullptr;
  }

  return *this;
}

void Timer::start() {
  assert(dispatcher != nullptr);
  assert(stopped);
  stopped = false;
}

void Timer::stop() {
  assert(dispatcher != nullptr);
  assert(!stopped);
  if (context != nullptr) {
    Context* context2 = static_cast<Context*>(context);
    if (context2->context != nullptr) {
      if (CancelWaitableTimer(timer) != TRUE) {
        std::cerr << "CancelWaitableTimer failed, result=" << GetLastError() << '.' << std::endl;
        throw std::runtime_error("Timer::stop");
      }

      dispatcher->pushContext(context2->context);
      context2->context = nullptr;
      context2->interrupted = true;
    }
  }

  stopped = true;
}

void Timer::sleep(std::chrono::nanoseconds duration) {
  assert(dispatcher != nullptr);
  assert(context == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  LARGE_INTEGER duration2;
  duration2.QuadPart = static_cast<LONGLONG>(duration.count() / -100);
  Context context2 = {dispatcher, GetCurrentFiber(), false};
  if (SetWaitableTimer(timer, &duration2, 0, callbackProcedure, &context2, FALSE) != TRUE) {
    std::cerr << "SetWaitableTimer failed, result=" << GetLastError() << '.' << std::endl;
    throw std::runtime_error("Timer::sleep");
  }

  context = &context2;
  dispatcher->yield();
  assert(dispatcher != nullptr);
  assert(context2.context == nullptr);
  assert(context == &context2);
  context = nullptr;
  if (context2.interrupted) {
    throw InterruptedException();
  }
}
