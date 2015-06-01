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

#include "Event.h"
#include <cassert>
#include <System/Dispatcher.h>

namespace System {

namespace {

struct EventWaiter {
  EventWaiter* next;
  void* context;
};

}

Event::Event() : dispatcher(nullptr) {
}

Event::Event(Dispatcher& dispatcher) : dispatcher(&dispatcher), state(false), first(nullptr) {
}

Event::Event(Event&& other) : dispatcher(other.dispatcher) {
  if (dispatcher != nullptr) {
    state = other.state;
    if (!state) {
      assert(other.first == nullptr);
      first = nullptr;
    }

    other.dispatcher = nullptr;
  }
}

Event::~Event() {
  assert(dispatcher == nullptr || state || first == nullptr);
}

Event& Event::operator=(Event&& other) {
  assert(dispatcher == nullptr || state || first == nullptr);
  dispatcher = other.dispatcher;
  if (dispatcher != nullptr) {
    state = other.state;
    if (!state) {
      assert(other.first == nullptr);
      first = nullptr;
    }

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
  if (state) {
    state = false;
    first = nullptr;
  }
}

void Event::set() {
  assert(dispatcher != nullptr);
  if (!state) {
    state = true;
    for (EventWaiter* waiter = static_cast<EventWaiter*>(first); waiter != nullptr; waiter = waiter->next) {
      dispatcher->pushContext(waiter->context);
    }
  }
}

void Event::wait() {
  assert(dispatcher != nullptr);
  if (!state) {
    EventWaiter waiter = {nullptr, dispatcher->getCurrentContext()};
    if (first != nullptr) {
      static_cast<EventWaiter*>(last)->next = &waiter;
    } else {
      first = &waiter;
    }

    last = &waiter;
    dispatcher->dispatch();
    assert(waiter.context == dispatcher->getCurrentContext());
    assert(dispatcher != nullptr);
  }
}

}
