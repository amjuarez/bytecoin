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

#include "Latch.h"
#include <cassert>
#include <System/Dispatcher.h>

namespace System {

namespace {

struct LatchWaiter {
  LatchWaiter* next;
  void* context;
};

}

Latch::Latch() : dispatcher(nullptr) {
}

Latch::Latch(Dispatcher& dispatcher) : dispatcher(&dispatcher), value(0) {
}

Latch::Latch(Latch&& other) : dispatcher(other.dispatcher) {
  if (dispatcher != nullptr) {
    value = other.value;
    if (value > 0) {
      assert(other.first == nullptr);
      first = nullptr;
    }

    other.dispatcher = nullptr;
  }
}

Latch::~Latch() {
  assert(dispatcher == nullptr || value == 0 || first == nullptr);
}

Latch& Latch::operator=(Latch&& other) {
  assert(dispatcher == nullptr || value == 0 || first == nullptr);
  dispatcher = other.dispatcher;
  if (dispatcher != nullptr) {
    value = other.value;
    if (value > 0) {
      assert(other.first == nullptr);
      first = nullptr;
    }

    other.dispatcher = nullptr;
  }

  return *this;
}

std::size_t Latch::get() const {
  assert(dispatcher != nullptr);
  return value;
}

void Latch::increase(std::size_t value) {
  assert(dispatcher != nullptr);
  if (value > 0) {
    if (this->value == 0) {
      first = nullptr;
    }

    this->value += value;
  }
}

void Latch::decrease(std::size_t value) {
  assert(dispatcher != nullptr);
  if (value > 0) {
    assert(value <= this->value);
    if (this->value > 0) {
      this->value -= value;
      if (this->value == 0) {
        for (LatchWaiter* waiter = static_cast<LatchWaiter*>(first); waiter != nullptr; waiter = waiter->next) {
          dispatcher->pushContext(waiter->context);
        }
      }
    }
  }
}

void Latch::wait() {
  assert(dispatcher != nullptr);
  if (value > 0) {
    LatchWaiter waiter = {nullptr, dispatcher->getCurrentContext()};
    if (first != nullptr) {
      static_cast<LatchWaiter*>(last)->next = &waiter;
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
