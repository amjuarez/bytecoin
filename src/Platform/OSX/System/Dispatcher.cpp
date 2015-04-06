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
#include <iostream>
#define _XOPEN_SOURCE
#include <ucontext.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <assert.h>
#include <sys/time.h>

using namespace System;

void Dispatcher::contextProcedureStatic(void *context) {
  reinterpret_cast<Dispatcher*>(context)->contextProcedure();
}

Dispatcher::Dispatcher() : lastCreatedTimer(0) {
  kqueue = ::kqueue();
  if (kqueue == -1) {
    std::cerr << "kqueue() fail errno=" << errno << std::endl;
  } else {
    currentContext = new ucontext_t;
    if (getcontext(reinterpret_cast<ucontext_t*>(currentContext)) == -1) {
      std::cerr << "getcontext() fail errno=" << errno << std::endl;
    } else {
      contextCount = 0;
      return;
    }
  }
  throw std::runtime_error("Dispatcher::Dispatcher");
}

Dispatcher::~Dispatcher() {
  assert(resumingContexts.empty());
  assert(reusableContexts.size() == contextCount);
  assert(spawningProcedures.empty());
  assert(reusableContexts.size() == allocatedStacks.size());
  while (!reusableContexts.empty()) {
    delete[] allocatedStacks.top();
    allocatedStacks.pop();
    delete   static_cast<ucontext_t*>(reusableContexts.top());
    reusableContexts.pop();
  }

  while (!timers.empty()) {
    timers.pop();
  }

  if (-1 == close(kqueue)) {
    std::cerr << "close() fail errno=" << errno << std::endl;
  }
}

void* Dispatcher::getCurrentContext() const {
  return currentContext;
}

int Dispatcher::getKqueue() const {
  return kqueue;
}

void Dispatcher::pushContext(void* context) {
  resumingContexts.push(context);
}

void Dispatcher::spawn(std::function<void()>&& procedure) {
  void* context;
  if (reusableContexts.empty()) {
    context = new ucontext_t;
    if (-1 == getcontext(reinterpret_cast<ucontext_t *>(context))) { //makecontext precondition
      std::cerr << "getcontext() fail errno=" << errno << std::endl;
      throw std::runtime_error("Dispatcher::spawn()");
    }
    auto stackPointer = new uint8_t[64 * 1024];
    reinterpret_cast<ucontext_t *>(context)->uc_stack.ss_sp = stackPointer;
    allocatedStacks.push(stackPointer);
    reinterpret_cast<ucontext_t *>(context)->uc_stack.ss_size = 64 * 1024;
    makecontext(reinterpret_cast<ucontext_t *>(context), (void(*)())contextProcedureStatic, 1, reinterpret_cast<int*>(this));
    ++contextCount;
  } else {
    context = reusableContexts.top();
    reusableContexts.pop();
  }

  resumingContexts.push(context);
  spawningProcedures.emplace(std::move(procedure));
}

int Dispatcher::getTimer() {
  int timer;
  if (timers.empty()) {
    timer = ++lastCreatedTimer;
  } else {
    timer = timers.top();
    timers.pop();
  }

  return timer;
}

void Dispatcher::pushTimer(int timer) {
  timers.push(timer);
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

    struct kevent event;
    int count = kevent(kqueue, NULL, 0, &event, 1, NULL);

    if (count == 1) {
      context = static_cast<ContextExt*>(event.udata)->context;
      break;
    }

    if (errno != EINTR) {
      std::cerr << "kevent() failed, errno=" << errno << std::endl;
      throw std::runtime_error("Dispatcher::yield()");
    }
  }

  if (context != currentContext) {
    ucontext_t* oldContext = static_cast<ucontext_t*>(currentContext);
    currentContext = context;
    if (-1 == swapcontext(oldContext, static_cast<ucontext_t *>(context))) {
      std::cerr << "setcontext() failed, errno=" << errno << std::endl;
      throw std::runtime_error("Dispatcher::yield()");
    }
  }
}

void Dispatcher::contextProcedure() {
  void* context = currentContext;
  for (;;) {
    assert(!spawningProcedures.empty());
    std::function<void()> procedure = std::move(spawningProcedures.front());
    spawningProcedures.pop();
    procedure();
    reusableContexts.push(context);
    yield();
  }
}
