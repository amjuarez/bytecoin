// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Event.h"
#include <cassert>
#include <iostream>
#include "Dispatcher.h"

using namespace System;

namespace {

struct Waiter {
  Waiter* next;
  void* context;
};

}

Event::Event() : dispatcher(nullptr) {
}

Event::Event(Dispatcher& dispatcher) : dispatcher(&dispatcher), first(nullptr), state(false) {
}

Event::Event(Event&& other) : dispatcher(other.dispatcher) {
  if (other.dispatcher != nullptr) {
    first = other.first;
    if (other.first != nullptr) {
      last = other.last;
    }

    state = other.state;
    other.dispatcher = nullptr;
  }
}

Event::~Event() {
  assert(first == nullptr);
}

Event& Event::operator=(Event&& other) {
  assert(first == nullptr);
  dispatcher = other.dispatcher;
  if (other.dispatcher != nullptr) {
    first = other.first;
    if (other.first != nullptr) {
      last = other.last;
    }

    state = other.state;
    other.dispatcher = nullptr;
  }

  return *this;
}

bool Event::get() const {
  assert(dispatcher != nullptr);
  return state;
}

void Event::clear() {
  assert(dispatcher != nullptr);
  state = false;
}

void Event::set() {
  assert(dispatcher != nullptr);
  state = true;
  for (Waiter* waiter = static_cast<Waiter*>(first); waiter != nullptr; waiter = waiter->next) {
    dispatcher->pushContext(waiter->context);
  }

  first = nullptr;
}

void Event::wait() {
  assert(dispatcher != nullptr);
  if (!state) {
    Waiter waiter = { nullptr, dispatcher->getCurrentContext() };
    if (first != nullptr) {
      static_cast<Waiter*>(last)->next = &waiter;
    } else {
      first = &waiter;
    }

    last = &waiter;
    dispatcher->yield();
    assert(dispatcher != nullptr);
  }
}
