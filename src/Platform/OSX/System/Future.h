// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

namespace System {

namespace Detail {

namespace { 

enum class State : unsigned {
  STARTED,
  COMPLETED,
  CONSUMED
};

}

// Simplest possible future implementation. The reason why this class even exist is because currenty std future has a
// memory corrupting bug on OSX. Spawn a new thread, execute procedure in it, get result, and wait thread to shut down.
// Actualy, this is what libstdc++'s std::future is doing.
template<class T> class Future {
public:
  // Create a new thread, and run `operation` in it.
  explicit Future(std::function<T()>&& operation) : procedure(std::move(operation)), state(State::STARTED), worker{[this] { asyncOp(); }} {
  }

  // Wait for async op to complete, then if thread wasn't detached, join it.
  ~Future() {
    wait();
    if (worker.joinable()) {
      worker.join();
    }
  }

  // Get result of async operation. UB if called more than once.
  T get() const {
    assert(state != State::CONSUMED);
    wait();
    state = State::CONSUMED;
    if (currentException != nullptr) {
      std::rethrow_exception(currentException);
    }

    return std::move(result);
  }

  // Wait for async operation to complete, if op is already completed, return immediately.
  void wait() const {
    std::unique_lock<std::mutex> guard(operationMutex);
    while (state == State::STARTED) {
      operationCondition.wait(guard);
    }
  }

  bool valid() const {
    std::unique_lock<std::mutex> guard(operationMutex);
    return state != State::CONSUMED;
  }

private:
  // This function is executed in a separate thread.
  void asyncOp() {
    try {
      assert(procedure != nullptr);
      result = procedure();
    } catch (...) {
      currentException = std::current_exception();
    }

    std::unique_lock<std::mutex> guard(operationMutex);
    state = State::COMPLETED;
    operationCondition.notify_one();
  }

  mutable T result;
  std::function<T()> procedure;
  std::exception_ptr currentException;
  mutable std::mutex operationMutex;
  mutable std::condition_variable operationCondition;
  mutable State state;
  std::thread worker;
};

template<> class Future<void> {
public:
  // Create a new thread, and run `operation` in it.
  explicit Future(std::function<void()>&& operation) : procedure(std::move(operation)), state(State::STARTED), worker{[this] { asyncOp(); }} {
  }

  // Wait for async op to complete, then if thread wasn't detached, join it.
  ~Future() {
    wait();
    if (worker.joinable()) {
      worker.join();
    }
  }

  // Get result of async operation. UB if called more than once.
  void get() const {
    assert(state != State::CONSUMED);
    wait();
    state = State::CONSUMED;
    if (currentException != nullptr) {
      std::rethrow_exception(currentException);
    }
  }

  // Wait for async operation to complete, if op is already completed, return immediately.
  void wait() const {
    std::unique_lock<std::mutex> guard(operationMutex);
    while (state == State::STARTED) {
      operationCondition.wait(guard);
    }
  }

  bool valid() const {
    std::unique_lock<std::mutex> guard(operationMutex);
    return state != State::CONSUMED;
  }

private:
  // This function is executed in a separate thread.
  void asyncOp() {
    try {
      assert(procedure != nullptr);
      procedure();
    } catch (...) {
      currentException = std::current_exception();
    }

    std::unique_lock<std::mutex> guard(operationMutex);
    state = State::COMPLETED;
    operationCondition.notify_one();
  }

  std::function<void()> procedure;
  std::exception_ptr currentException;
  mutable std::mutex operationMutex;
  mutable std::condition_variable operationCondition;
  mutable State state;
  std::thread worker;
};

template<class T> std::function<T()> async(std::function<T()>&& operation) {
  return operation;
}

}

}
