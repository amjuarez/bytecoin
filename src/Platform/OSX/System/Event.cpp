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
