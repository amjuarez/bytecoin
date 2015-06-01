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

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <ucontext.h>
#include <unistd.h>
#include <fcntl.h>

namespace System {

Dispatcher::Dispatcher() {
  std::string message;
  epoll = ::epoll_create1(0);
  if (epoll == -1) {
    message = "epoll_create1() fail errno=" + std::to_string(errno);
  } else {
    currentContext = new ucontext_t;
    if (getcontext(reinterpret_cast<ucontext_t*>(currentContext)) == -1) {
      message = "getcontext() fail errno=" + std::to_string(errno);
    } else {
      remoteSpawnEvent = eventfd(0, O_NONBLOCK);
      if(remoteSpawnEvent == -1) {
        message = "eventfd() fail errno=" + std::to_string(errno);
      } else {
        eventContext.writeContext = nullptr;
        eventContext.readContext = nullptr;

        epoll_event remoteSpawnEventEpollEvent;
        remoteSpawnEventEpollEvent.events = EPOLLIN;
        remoteSpawnEventEpollEvent.data.ptr = &eventContext;

        if (epoll_ctl(epoll, EPOLL_CTL_ADD, remoteSpawnEvent, &remoteSpawnEventEpollEvent) == -1) {
          message = "epoll_ctl() failed, errno=" + std::to_string(errno);
        } else {
          contextCount = 0;
          *reinterpret_cast<pthread_mutex_t*>(this->mutex) = pthread_mutex_t(PTHREAD_MUTEX_INITIALIZER);
          return;
        }

        auto result = close(remoteSpawnEvent);
        assert(result == 0);
      }
    }

    auto result = close(epoll);
    assert(result == 0);
  }

  throw std::runtime_error("Dispatcher::Dispatcher, "+message);
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
    int result = ::close(timers.top());
    assert(result == 0);
    timers.pop();
  }

  auto result = close(epoll);
  assert(result == 0);
  result = close(remoteSpawnEvent);
  assert(result == 0);
  result = pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(this->mutex));
  assert(result == 0);
}

void Dispatcher::clear() {
  while (!reusableContexts.empty()) {
    delete[] allocatedStacks.top();
    allocatedStacks.pop();
    delete static_cast<ucontext_t*>(reusableContexts.top());
    reusableContexts.pop();
    --contextCount;
  }

  while (!timers.empty()) {
    int result = ::close(timers.top());
    if (result == -1) {
      throw std::runtime_error("Dispatcher::clear, close failed, errno="  + std::to_string(errno));
    }

    timers.pop();
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

    epoll_event event;
    int count = epoll_wait(epoll, &event, 1, -1);
    if (count == 1) {
      ContextPair *contextPair = static_cast<ContextPair*>(event.data.ptr);
      if(((event.events & (EPOLLIN | EPOLLOUT)) != 0) && contextPair->readContext == nullptr && contextPair->writeContext == nullptr) {
        uint64_t buf;
        auto transferred = read(remoteSpawnEvent, &buf, sizeof buf);
        if(transferred == -1) {
            throw std::runtime_error("Dispatcher::dispatch() read(remoteSpawnEvent) fail errno=" + std::to_string(errno));
        }

        pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
        while (!remoteSpawningProcedures.empty()) {
          spawn(std::move(remoteSpawningProcedures.front()));
          remoteSpawningProcedures.pop();
        }

        pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
        continue;
      }

      if ((event.events & EPOLLOUT) != 0) {
        context = contextPair->writeContext->context;
        contextPair->writeContext->events = event.events;
      } else if ((event.events & EPOLLIN) != 0) {
        context = contextPair->readContext->context;
        contextPair->readContext->events = event.events;
      } else {
        continue;
      }

      assert(context != nullptr);
      break;
    }

    if (errno != EINTR) {
      throw std::runtime_error("Dispatcher::dispatch(), epoll_wait() failed, errno="  + std::to_string(errno));
    }
  }

  if (context != currentContext) {
    ucontext_t* oldContext = static_cast<ucontext_t*>(currentContext);
    currentContext = context;
    if (swapcontext(oldContext, static_cast<ucontext_t *>(context)) == -1) {
      throw std::runtime_error("Dispatcher::dispatch()  swapcontext() failed, errno=" + std::to_string(errno));
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
  pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
  uint64_t one = 1;
  auto transferred = write(remoteSpawnEvent, &one, sizeof one);
  if(transferred == - 1) {
    throw std::runtime_error("Dispatcher::remoteSpawn, write() failed errno = " + std::to_string(errno));
  }
}

void Dispatcher::spawn(std::function<void()>&& procedure) {
  ucontext_t *context;
  if (reusableContexts.empty()) {
    context = new ucontext_t;
    if (getcontext(context) == -1) { //makecontext precondition
      throw std::runtime_error("Dispatcher::spawn(), getcontext() fail errno=" + std::to_string(errno));
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

void Dispatcher::yield() {
  for(;;){
    epoll_event events[16];
    int count = epoll_wait(epoll, events, 16, 0);
    if (count == 0) {
      break;
    }

    if(count > 0) {
      for(int i = 0; i < count; ++i) {
        ContextPair *contextPair = static_cast<ContextPair*>(events[i].data.ptr);
        if(((events[i].events & (EPOLLIN | EPOLLOUT)) != 0) && contextPair->readContext == nullptr && contextPair->writeContext == nullptr) {
          uint64_t buf;
          auto transferred = read(remoteSpawnEvent, &buf, sizeof buf);
          if(transferred == -1) {
            throw std::runtime_error("Dispatcher::dispatch() read(remoteSpawnEvent) fail errno=" + std::to_string(errno));
          }

          pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
          while (!remoteSpawningProcedures.empty()) {
            spawn(std::move(remoteSpawningProcedures.front()));
            remoteSpawningProcedures.pop();
          }

          pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(this->mutex));
          continue;
        }

        if ((events[i].events & EPOLLOUT) != 0) {
          resumingContexts.push(contextPair->writeContext->context);
          contextPair->writeContext->events = events[i].events;
        } else if ((events[i].events & EPOLLIN) != 0) {
          resumingContexts.push(contextPair->readContext->context);
          contextPair->readContext->events = events[i].events;
        } else {
          continue;
        }
      }
    } else {
      if (errno != EINTR) {
        throw std::runtime_error("Dispatcher::dispatch(), epoll_wait() failed, errno=" + std::to_string(errno));
      }
    }
  }

  if(!resumingContexts.empty()){
    resumingContexts.push(getCurrentContext());
    dispatch();
  }
}

int Dispatcher::getEpoll() const {
  return epoll;
}

int Dispatcher::getTimer() {
  int timer;
  if (timers.empty()) {
    timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    epoll_event timerEvent;
    timerEvent.events = 0;
    timerEvent.data.ptr = nullptr;

    if (epoll_ctl(getEpoll(), EPOLL_CTL_ADD, timer, &timerEvent) == -1) {
      throw std::runtime_error("Dispatcher::getTimer(), epoll_ctl() failed, errno="  + std::to_string(errno));
    }
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

void Dispatcher::contextProcedureStatic(void *context) {
  reinterpret_cast<Dispatcher*>(context)->contextProcedure();
}

}
