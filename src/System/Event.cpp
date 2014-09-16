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
#include "System.h"

struct Event::Waiter {
  Event::Waiter* next;
  void* context;
};

Event::Event() : system(nullptr) {
}

Event::Event(System& system) : system(&system), first(nullptr), state(false) {
}

Event::Event(Event&& other) : system(other.system) {
  if (other.system != nullptr) {
    first = other.first;
    if (other.first != nullptr) {
      last = other.last;
    }

    state = other.state;
    other.system = nullptr;
  }
}

Event::~Event() {
  assert(first == nullptr);
}

Event& Event::operator=(Event&& other) {
  assert(first == nullptr);
  system = other.system;
  if (other.system != nullptr) {
    first = other.first;
    if (other.first != nullptr) {
      last = other.last;
    }

    state = other.state;
    other.system = nullptr;
  }

  return *this;
}

bool Event::get() const {
  assert(system != nullptr);
  return state;
}

void Event::clear() {
  assert(system != nullptr);
  state = false;
}

void Event::set() {
  assert(system != nullptr);
  state = true;
  for (Waiter* waiter = first; waiter != nullptr; waiter = waiter->next) {
    system->pushContext(waiter->context);
  }

  first = nullptr;
}

void Event::wait() {
  assert(system != nullptr);
  Waiter waiter = {nullptr, system->getCurrentContext()};
  if (first != nullptr) {
    last->next = &waiter;
  } else {
    first = &waiter;
  }

  last = &waiter;
  while (!state) {
    system->yield();
  }
}
