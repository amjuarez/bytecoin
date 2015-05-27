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

#include "Dispatcher.h"
#include <cassert>
#include <string>

#include <fcntl.h>
#include <pthread.h>
#include <sys/event.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "context.h"

namespace System {

Dispatcher::Dispatcher() : lastCreatedTimer(0) {
  std::string message;
  kqueue = ::kqueue();
  if (kqueue == -1) {
    message = "kqueue() fail errno=" + std::to_string(errno);
  } else {
    currentContext = new uctx;
    if (getcontext(static_cast<uctx*>(currentContext)) == -1) {
      message = "getcontext() fail errno=" + std::to_string(errno);
    } else {
      struct kevent event;
      EV_SET(&event, 0, EVFILT_USER, EV_ADD, NOTE_FFNOP, 0, NULL);
      if (kevent(kqueue, &event, 1, NULL, 0, NULL) == -1) {
        message = "kevent() fail errno=" + std::to_string(errno);
      } else {
        if(pthread_mutex_init(reinterpret_cast<pthread_mutex_t*>(this->mutex), NULL) == -1) {
          message = "pthread_mutex_init() fail errno=" + std::to_string(errno);
        } else {
          remoteSpawned = false;
          contextCount = 0;
          return;
        }
      }
    }

    auto result = close(kqueue);
    assert(result == 0);
  }

  throw std::runtime_error("Dispatcher::Dispatcher, " + message);
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

  auto result = close(kqueue);
  assert(result != -1);
  result = pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(this->mutex));
  assert(result != -1);
}

void Dispatcher::clear() {
  while (!reusableContexts.empty()) {
    delete[] allocatedStacks.top();
    allocatedStacks.pop();
    delete static_cast<ucontext_t*>(reusableContexts.top());
    reusableContexts.pop();
    --contextCount;
  }
}

void Dispatcher::dispatch() {
  void* context;
  for (;;) {
    if (!resumingContexts.empty()) {
      context = resumingContexts.front();
      resumingContexts.pop();
      break;
    }

    if(remoteSpawned.load() == true) {
      pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
      while (!remoteSpawningProcedures.empty()) {
        spawn(std::move(remoteSpawningProcedures.front()));
        remoteSpawningProcedures.pop();
      }

      remoteSpawned = false;
      pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
      continue;
    }

    struct kevent event;
    int count = kevent(kqueue, NULL, 0, &event, 1, NULL);
    if (count == 1) {
      if (event.filter == EVFILT_USER && event.ident == 0) {
        struct kevent event;
        EV_SET(&event, 0, EVFILT_USER, EV_ADD | EV_DISABLE, NOTE_FFNOP, 0, NULL);
        if (kevent(kqueue, &event, 1, NULL, 0, NULL) == -1) {
          throw std::runtime_error("kevent() fail errno=" + std::to_string(errno));
        }

        continue;
      }

      context = static_cast<OperationContext*>(event.udata)->context;
      break;
    }

    if (errno != EINTR) {
      throw std::runtime_error("Dispatcher::dispatch(), kqueue() fail errno=" + std::to_string(errno));
    } else {
      pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
      while (!remoteSpawningProcedures.empty()) {
        spawn(std::move(remoteSpawningProcedures.front()));
        remoteSpawningProcedures.pop();
      }

      pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
    }
  }

  if (context != currentContext) {
    uctx* oldContext = static_cast<uctx*>(currentContext);
    currentContext = context;
    if (swapcontext(oldContext,static_cast<uctx*>(currentContext)) == -1) {
      throw std::runtime_error("Dispatcher::dispatch(), swapcontext() failed, errno=" + std::to_string(errno));
    }
  }
}

void* Dispatcher::getCurrentContext() const {
  return currentContext;
}

void Dispatcher::pushContext(void* context) {
  resumingContexts.push(context);
}

void Dispatcher::remoteSpawn(std::function<void()>&& procedure) {
  pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
  remoteSpawningProcedures.push(std::move(procedure));
  if(remoteSpawned == false) {
    remoteSpawned = true;
    struct kevent event;
    EV_SET(&event, 0, EVFILT_USER, EV_ADD | EV_ONESHOT, NOTE_FFCOPY | NOTE_TRIGGER, 0, NULL);
    if (kevent(kqueue, &event, 1, NULL, 0, NULL) == -1) {
      throw std::runtime_error("Dispatcher::remoteSpawn(), kevent() fail errno=" + std::to_string(errno));
    };
  }

  pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
}

void Dispatcher::spawn(std::function<void()>&& procedure) {
  void* context;
  if (reusableContexts.empty()) {
    context = new uctx;
    uint8_t* stackPointer = new uint8_t[64 * 1024];
    allocatedStacks.push(stackPointer);

    static_cast<uctx*>(context)->uc_stack.ss_sp = stackPointer;
    static_cast<uctx*>(context)->uc_stack.ss_size = 64 * 1024;
    makecontext(static_cast<uctx*>(context), reinterpret_cast<void(*)()>(contextProcedureStatic), reinterpret_cast<intptr_t>(this));
    
    ++contextCount;
  } else {
    context = reusableContexts.top();
    reusableContexts.pop();
  }

  resumingContexts.push(context);
  spawningProcedures.emplace(std::move(procedure));
}

void Dispatcher::yield() {
  struct timespec zeroTimeout = { 0, 0 };
  for (;;) {
    struct kevent events[16];
    int count = kevent(kqueue, NULL, 0, events, 16, &zeroTimeout);
    if (count == 0) {
      break;
    }

    if (count > 0) {
      for (int i = 0; i < count; ++i) {
        if (events[i].filter == EVFILT_USER && events[i].ident == 0) {
          struct kevent event;
          EV_SET(&event, 0, EVFILT_USER, EV_ADD | EV_DISABLE, NOTE_FFNOP, 0, NULL);
          if (kevent(kqueue, &event, 1, NULL, 0, NULL) == -1) {
            throw std::runtime_error("kevent() fail errno=" + std::to_string(errno));
          }
          
          pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
          while (!remoteSpawningProcedures.empty()) {
            spawn(std::move(remoteSpawningProcedures.front()));
            remoteSpawningProcedures.pop();
          }

          remoteSpawned = false;
          pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
          continue;
        }

        resumingContexts.push(static_cast<OperationContext*>(events[i].udata)->context);
      }
    } else {
      if (errno != EINTR) {
        throw std::runtime_error("Dispatcher::dispatch(), epoll_wait() failed, errno=" + std::to_string(errno));
      }
    }
  }

  if (!resumingContexts.empty()) {
    resumingContexts.push(getCurrentContext());
    dispatch();
  }
}

int Dispatcher::getKqueue() const {
  return kqueue;
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

void Dispatcher::contextProcedure() {
  void* context = currentContext;
  for (;;) {
    assert(!spawningProcedures.empty());
    std::function<void()> procedure = std::move(spawningProcedures.front());
    spawningProcedures.pop();
    procedure();
    reusableContexts.push(context);
    dispatch();
  }
}

void Dispatcher::contextProcedureStatic(intptr_t context) {
  reinterpret_cast<Dispatcher*>(context)->contextProcedure();
}

}
