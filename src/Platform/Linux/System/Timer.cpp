// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Timer.h"
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <errno.h>
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
  timer = timerfd_create(CLOCK_MONOTONIC, 0);
  epoll_event timerEvent;
  timerEvent.data.fd = timer;
  timerEvent.events = 0;
  timerEvent.data.ptr = nullptr;

  if (epoll_ctl(this->dispatcher->getEpoll(), EPOLL_CTL_ADD, timer, &timerEvent) == -1) {
    std::cerr << "epoll_ctl() failed, errno=" << errno << '.' << std::endl;
    throw std::runtime_error("Timer::Timer");
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

Timer::~Timer() {
  if (dispatcher != nullptr) {
    close(timer);
  }
}

Timer& Timer::operator=(Timer&& other) {
  if (dispatcher != nullptr) {
    assert(context == nullptr);
    close(timer);
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

  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);

  itimerspec expires;
  expires.it_interval.tv_nsec = expires.it_interval.tv_sec = 0;
  expires.it_value.tv_sec = seconds.count();
  expires.it_value.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds).count();
  timerfd_settime(timer, 0, &expires, NULL);

  TimerContext context2;
  context2.dispatcher = dispatcher;
  context2.context = dispatcher->getCurrentContext();
  context2.writeContext = nullptr;
  context2.interrupted = false;

  epoll_event timerEvent;
  timerEvent.data.fd = timer;
  timerEvent.events = EPOLLIN | EPOLLONESHOT;
  timerEvent.data.ptr = &context2;

  if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, timer, &timerEvent) == -1) {
    std::cerr << "epoll_ctl() failed, errno=" << errno << '.' << std::endl;
    throw std::runtime_error("Timer::sleep");
  }

  context = &context2;
  dispatcher->yield();
  assert(dispatcher != nullptr);
  assert(context2.context == dispatcher->getCurrentContext());
  assert(context2.writeContext == nullptr);
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
      epoll_event timerEvent;
      timerEvent.data.fd = timer;
      timerEvent.events = 0;
      timerEvent.data.ptr = nullptr;

      if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, timer, &timerEvent) == -1) {
        std::cerr << "epoll_ctl() failed, errno=" << errno << '.' << std::endl;
        throw std::runtime_error("Timer::sleep");
      }

      dispatcher->pushContext(context2->context);
      context2->interrupted = true;
    }
  }

  stopped = true;
}
