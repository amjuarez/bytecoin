// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ErrorMessage.h"

#include <cstddef>
#include <windows.h>

namespace System {

std::string lastErrorMessage() {
  return errorMessage(GetLastError());
}

std::string errorMessage(int error) {
  struct Buffer {
    ~Buffer() {
      if (pointer != nullptr) {
        LocalFree(pointer);
      }
    }

    LPTSTR pointer = nullptr;
  } buffer;

  auto size = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr, error,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&buffer.pointer), 0, nullptr);
  return "result=" + std::to_string(error) + ", " + std::string(buffer.pointer, size);
}

}
