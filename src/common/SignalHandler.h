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
