// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Timer.h"
#include <cassert>
#include <iostream>
#include <sys/event.h>
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>
#include "Dispatcher.h"
#include "InterruptedException.h"

using namespace System;

namespace {

struct TimerContext : public Dispatcher::ContextExt {
  Dispatcher* dispatcher;
  bool interrupted;
};

}

Timer::Timer() : dispatcher(nullptr) {
}

Timer::Timer(Dispatcher& dispatcher) : dispatcher(&dispatcher), stopped(false), context(nullptr) {
  timer = dispatcher.getTimer();
}

Timer::~Timer() {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    dispatcher->pushTimer(timer);
  }
}

Timer::Timer(Timer&& other) : dispatcher(other.dispatcher) {
  if (other.dispatcher != nullptr) {
    timer = other.timer;
    stopped = other.stopped;
    context = other.context;
    other.dispatcher = nullptr;
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

void Timer::sleep(std::chrono::milliseconds duration) {
  assert(dispatcher != nullptr);
  assert(context == nullptr);
  if (stopped) {
    throw InterruptedException();
  }

  TimerContext context2;
  context2.dispatcher = dispatcher;
  context2.context = dispatcher->getCurrentContext();
  context2.interrupted = false;

  struct kevent event;
  EV_SET(&event, timer, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, duration.count(), &context2);

  if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
    std::cerr << "kevent() failed, errno=" << errno << '.' << std::endl;
    throw std::runtime_error("Timer::sleep");
  }

  context = &context2;
  dispatcher->yield();
  assert(dispatcher != nullptr);
  assert(context2.context == dispatcher->getCurrentContext());
  assert(context == &context2);
  context = nullptr;
  context2.context = nullptr;
  if (context2.interrupted) {
    throw InterruptedException();
  }
}

void Timer::stop() {
  assert(dispatcher != nullptr);
  assert(!stopped);
  if (context != nullptr) {
    TimerContext* context2 = reinterpret_cast<TimerContext*>(context);
    if (context2->context != nullptr) {
      struct kevent event;
      EV_SET(&event, timer, EVFILT_TIMER, EV_DISABLE, 0, 0, NULL);

      if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
        std::cerr << "kevent() failed, errno=" << errno << '.' << std::endl;
        throw std::runtime_error("Timer::stop");
      }

      dispatcher->pushContext(context2->context);
      context2->interrupted = true;
    }
  }

  stopped = true;
}
