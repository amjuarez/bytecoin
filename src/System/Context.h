// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/InterruptedException.h>

namespace System {

template<typename ResultType = void>
class Context {
public:
  Context(Dispatcher& dispatcher, std::function<ResultType()>&& target) : 
    dispatcher(dispatcher), target(std::move(target)), ready(dispatcher), bindingContext(dispatcher.getReusableContext()) {
    bindingContext.interrupted = false;
    bindingContext.groupNext = nullptr;
    bindingContext.groupPrev = nullptr;
    bindingContext.group = nullptr;
    bindingContext.procedure = [this] {
      try {
        new(resultStorage) ResultType(this->target());
      } catch (...) {
        exceptionPointer = std::current_exception();
      }

      ready.set();
    };

    dispatcher.pushContext(&bindingContext);
  }

  Context(const Context&) = delete;  
  Context& operator=(const Context&) = delete;

  ~Context() {
    interrupt();
    wait();
    dispatcher.pushReusableContext(bindingContext);
  }

  ResultType& get() {
    wait();
    if (exceptionPointer != nullptr) {
      std::rethrow_exception(exceptionPointer);
    }

    return *reinterpret_cast<ResultType*>(resultStorage);
  }

  void interrupt() {
    dispatcher.interrupt(&bindingContext);
  }

  void wait() {
    for (;;) {
      try {
        ready.wait();
        break;
      } catch (InterruptedException&) {
        interrupt();
      }
    }
  }

private:
  uint8_t resultStorage[sizeof(ResultType)];
  Dispatcher& dispatcher;
  std::function<ResultType()> target;
  Event ready;
  NativeContext& bindingContext;
  std::exception_ptr exceptionPointer;
};

template<>
class Context<void> {
public:
  Context(Dispatcher& dispatcher, std::function<void()>&& target) :
    dispatcher(dispatcher), target(std::move(target)), ready(dispatcher), bindingContext(dispatcher.getReusableContext()) {
    bindingContext.interrupted = false;
    bindingContext.groupNext = nullptr;
    bindingContext.groupPrev = nullptr;
    bindingContext.group = nullptr;
    bindingContext.procedure = [this] {
      try {
        this->target();
      } catch (...) {
        exceptionPointer = std::current_exception();
      }

      ready.set();
    };

    dispatcher.pushContext(&bindingContext);
  }

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  ~Context() {
    interrupt();
    wait();
    dispatcher.pushReusableContext(bindingContext);
  }

  void get() {
    wait();
    if (exceptionPointer != nullptr) {
      std::rethrow_exception(exceptionPointer);
    }
  }

  void interrupt() {
    dispatcher.interrupt(&bindingContext);
  }

  void wait() {
    for (;;) {
      try {
        ready.wait();
        break;
      } catch (InterruptedException&) {
        interrupt();
      }
    }
  }

private:
  Dispatcher& dispatcher;
  std::function<void()> target;
  Event ready;
  NativeContext& bindingContext;
  std::exception_ptr exceptionPointer;
};

}
