// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

#include <functional>
#include <signal.h>

#include "misc_os_dependent.h"

namespace tools {
  class SignalHandler
  {
  public:
    template<typename T>
    static bool install(T t)
    {
#if defined(WIN32)
      bool r = TRUE == ::SetConsoleCtrlHandler(&winHandler, TRUE);
      if (r)
      {
        m_handler = t;
      }
      return r;
#else
      signal(SIGINT, posixHandler);
      signal(SIGTERM, posixHandler);
      m_handler = t;
      return true;
#endif
    }

  private:
#if defined(WIN32)
    static BOOL WINAPI winHandler(DWORD type);
#else
    static void posixHandler(int /*type*/);
#endif

    static void handleSignal();

  private:
    static std::function<void(void)> m_handler;
  };
}
