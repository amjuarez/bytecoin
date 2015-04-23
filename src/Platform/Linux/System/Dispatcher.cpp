// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Dispatcher.h"
#include <iostream>
#include <ucontext.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <assert.h>
#include <sys/time.h>
#include <errno.h>
#include <stdexcept>

using namespace System;

void Dispatcher::contextProcedureStatic(void *context) {
  reinterpret_cast<Dispatcher*>(context)->contextProcedure();
}

Dispatcher::Dispatcher() {
  epoll = ::epoll_create1(0);
  if (epoll == -1) {
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
    delete static_cast<ucontext_t*>(reusableContexts.top());
    reusableContexts.pop();
  }

  while (!timers.empty()) {
    timers.pop();
  }

  if (-1 == close(epoll)) {
    std::cerr << "close() fail errno=" << errno << std::endl;
  }
}

void* Dispatcher::getCurrentContext() const {
  return currentContext;
}

int Dispatcher::getEpoll() const {
  return epoll;
}

void Dispatcher::pushContext(void* context) {
  resumingContexts.push(context);
}

void Dispatcher::spawn(std::function<void()>&& procedure) {
  ucontext_t *context;
  if (reusableContexts.empty()) {
    context = new ucontext_t;
    if (getcontext(context) == -1) { //makecontext precondition
      std::cerr << "getcontext() fail errno=" << errno << std::endl;
      throw std::runtime_error("Dispatcher::spawn()");
    }
    auto stackPointer = new uint8_t[64 * 1024];
    context->uc_stack.ss_sp = stackPointer;
    allocatedStacks.push(stackPointer);
    context->uc_stack.ss_size = 64 * 1024;
    makecontext(context, (void(*)())contextProcedureStatic, 1, reinterpret_cast<int*>(this));
    ++contextCount;
  } else {
    context = static_cast<ucontext_t*>(reusableContexts.top());
    reusableContexts.pop();
  }

  resumingContexts.push(context);
  spawningProcedures.emplace(std::move(procedure));
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
      assert(context);
      break;
    }

    epoll_event event;
    int count = epoll_wait(epoll, &event, 1, -1);

    if (count == 1) {
      if ((event.events & EPOLLOUT) != 0) {
        context = static_cast<ContextExt *>(event.data.ptr)->writeContext;
      } else {
        context = static_cast<ContextExt *>(event.data.ptr)->context;
      }
      assert(context);
      break;
    }

    if (errno != EINTR) {
      std::cerr << "epoll_wait() failed, errno=" << errno << std::endl;
      throw std::runtime_error("Dispatcher::yield()");
    }
  }

  if (context != currentContext) {
    ucontext_t* oldContext = static_cast<ucontext_t*>(currentContext);
    currentContext = context;
    if (-1 == swapcontext(oldContext, static_cast<ucontext_t *>(context))) {
      std::cerr << "swapcontext() failed, errno=" << errno << std::endl;
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
