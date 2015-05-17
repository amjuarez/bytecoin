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

#pragma once

#include <functional>
#include <queue>
#include <stack>

class System {
public:
  System();
  System(const System&) = delete;
  ~System();
  System& operator=(const System&) = delete;
  void* getCurrentContext() const;
  void* getIoService();
  void pushContext(void* context);
  void spawn(std::function<void()>&& procedure);
  void yield();
  void wake();

  void contextProcedure();

private:
  void* ioService;
  void* work;
  std::stack<void*> contexts;
  std::queue<std::function<void()>> procedures;
  std::queue<void*> resumingContexts;
  void* currentContext;
};
