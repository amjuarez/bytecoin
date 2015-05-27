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

#pragma once

#include <cstddef>

namespace System {

class Dispatcher;

class Latch {
public:
  Latch();
  explicit Latch(Dispatcher& dispatcher);
  Latch(const Latch&) = delete;
  Latch(Latch&& other);
  ~Latch();
  Latch& operator=(const Latch&) = delete;
  Latch& operator=(Latch&& other);
  std::size_t get() const;
  void decrease(std::size_t value = 1);
  void increase(std::size_t value = 1);
  void wait();

private:
  Dispatcher* dispatcher;
  std::size_t value;
  void* first;
  void* last;
};

}
