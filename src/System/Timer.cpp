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

#include "Timer.h"
#include <boost/asio/steady_timer.hpp>
#include "System.h"

Timer::Timer() : system(nullptr) {
}

Timer::Timer(System& system) : system(&system) {
  timer = new boost::asio::steady_timer(*static_cast<boost::asio::io_service*>(system.getIoService()));
}

Timer::Timer(Timer&& other) : system(other.system) {
  if (other.system != nullptr) {
    timer = other.timer;
    other.system = nullptr;
  }
}

Timer::~Timer() {
  if (system != nullptr) {
    delete static_cast<boost::asio::steady_timer*>(timer);
  }
}

Timer& Timer::operator=(Timer&& other) {
  if (system != nullptr) {
    delete static_cast<boost::asio::steady_timer*>(timer);
  }

  system = other.system;
  if (other.system != nullptr) {
    timer = other.timer;
    other.system = nullptr;
  }

  return *this;
}

void Timer::sleep(std::chrono::milliseconds time) {
  assert(system != nullptr);
  static_cast<boost::asio::steady_timer*>(timer)->expires_from_now(time);
  void* context = system->getCurrentContext();
  boost::system::error_code errorCode;
  static_cast<boost::asio::steady_timer*>(timer)->async_wait([&](const boost::system::error_code& callbackErrorCode) {
    errorCode = callbackErrorCode;
    system->pushContext(context);
  });

  system->yield();
  if (errorCode) {
    throw boost::system::system_error(errorCode);
  }
}
