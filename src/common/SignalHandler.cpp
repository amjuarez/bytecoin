// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
