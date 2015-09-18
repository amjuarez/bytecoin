// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <exception>

namespace System {

class InterruptedException : public std::exception {
  public:
    const char* what() const throw() {
      return "interrupted";
    }
};

}
