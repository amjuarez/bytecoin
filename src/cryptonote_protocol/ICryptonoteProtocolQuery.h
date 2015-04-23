// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstddef>

namespace cryptonote {
class ICryptonoteProtocolObserver;

class ICryptonoteProtocolQuery {
public:
  virtual bool addObserver(ICryptonoteProtocolObserver* observer) = 0;
  virtual bool removeObserver(ICryptonoteProtocolObserver* observer) = 0;

  virtual uint64_t getObservedHeight() const = 0;
  virtual size_t getPeerCount() const = 0;
};

} //namespace cryptonote
