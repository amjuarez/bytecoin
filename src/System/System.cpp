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

#include "System.h"
#include <iostream>
#include <boost/asio/spawn.hpp>
#include <boost/context/fcontext.hpp>

namespace {
void contextProcedureStatic(intptr_t context) {
  reinterpret_cast<System*>(context)->contextProcedure();
}
}

System::System() {
  ioService = new boost::asio::io_service;
  work = new boost::asio::io_service::work(*static_cast<boost::asio::io_service*>(ioService));
  currentContext = new boost::context::fcontext_t;
}

System::~System() {
  assert(procedures.empty());
  assert(resumingContexts.empty());
  while (!contexts.empty()) {

    delete static_cast<boost::context::fcontext_t*>(contexts.top());

    contexts.pop();
  }
  delete static_cast<boost::asio::io_service::work*>(work);
  if (!static_cast<boost::asio::io_service*>(ioService)->stopped()) {
    static_cast<boost::asio::io_service*>(ioService)->stop();
  }
  delete static_cast<boost::asio::io_service*>(ioService);
}

void* System::getCurrentContext() const {

  return currentContext;
}

void* System::getIoService() {
  return ioService;
}

void System::pushContext(void* context) {
  resumingContexts.push(context);
}

void System::spawn(std::function<void()>&& procedure) {
  procedures.emplace(std::move(procedure));
}

void System::wake() {
  static_cast<boost::asio::io_service*>(ioService)->post([] {});
}

void System::yield() {
  if (procedures.empty()) {
    void* context;
    for (;;) {
      if (resumingContexts.empty()) {
        boost::system::error_code errorCode;
        static_cast<boost::asio::io_service*>(ioService)->run_one(errorCode);
        if (errorCode) {
          std::cerr << "boost::asio::io_service::run_onw failed, result=" << errorCode << '.' << std::endl;
          throw std::runtime_error("System::yield");
        }
      } else {
        context = resumingContexts.front();
        resumingContexts.pop();
        break;
      }
    }

    if (context != currentContext) {
      boost::context::fcontext_t* oldContext = static_cast<boost::context::fcontext_t*>(currentContext);
      currentContext = context;
#if (BOOST_VERSION >= 105600)
      boost::context::jump_fcontext(oldContext, *static_cast<boost::context::fcontext_t*>(context), reinterpret_cast<intptr_t>(this), false);
#else
      boost::context::jump_fcontext(oldContext, static_cast<boost::context::fcontext_t*>(context), reinterpret_cast<intptr_t>(this), false);
#endif
    }
  } else {
    void* context;
    if (contexts.empty()) {
#if (BOOST_VERSION >= 105600)
      context = new boost::context::fcontext_t(boost::context::make_fcontext(new uint8_t[65536] + 65536, 65536, contextProcedureStatic));
#else
      context = new boost::context::fcontext_t(*boost::context::make_fcontext(new uint8_t[65536] + 65536, 65536, contextProcedureStatic));
#endif
    } else {
      context = contexts.top();
      contexts.pop();
    }


    boost::context::fcontext_t* oldContext = static_cast<boost::context::fcontext_t*>(currentContext);
    currentContext = context;
#if (BOOST_VERSION >= 105600)
    boost::context::jump_fcontext(oldContext, *static_cast<boost::context::fcontext_t*>(context), reinterpret_cast<intptr_t>(this), false);
#else
    boost::context::jump_fcontext(oldContext, static_cast<boost::context::fcontext_t*>(context), reinterpret_cast<intptr_t>(this), false);
#endif
  }
}

void System::contextProcedure() {
  void* context = currentContext;
  for (;;) {
    assert(!procedures.empty());
    std::function<void()> procedure = std::move(procedures.front());
    procedures.pop();
    procedure();
    contexts.push(context);
    yield();
  }
}
