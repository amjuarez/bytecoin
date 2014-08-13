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

#include "SignalHandler.h"

#include <mutex>
#include <sstream>

// epee
#include "include_base_utils.h"


namespace tools {
  std::function<void(void)> SignalHandler::m_handler;

#if defined(WIN32)
  BOOL WINAPI SignalHandler::winHandler(DWORD type) {
    if (CTRL_C_EVENT == type || CTRL_BREAK_EVENT == type) {
      handleSignal();
      return TRUE;
    } else {
      LOG_PRINT_RED_L0("Got control signal " << type << ". Exiting without saving...");
      return FALSE;
    }
    return TRUE;
  }

#else

  void SignalHandler::posixHandler(int /*type*/) {
    handleSignal();
  }
#endif

  void SignalHandler::handleSignal() {
    static std::mutex m_mutex;
    std::unique_lock<std::mutex> lock(m_mutex);
    m_handler();
  }
}
