// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>

#include "cryptonote_protocol/ICryptonoteProtocolObserver.h"
#include "cryptonote_protocol/ICryptonoteProtocolQuery.h"

class ICryptonoteProtocolQueryStub: public cryptonote::ICryptonoteProtocolQuery {
public:
  ICryptonoteProtocolQueryStub() : peers(0), observedHeight(0) {}

  virtual bool addObserver(cryptonote::ICryptonoteProtocolObserver* observer);
  virtual bool removeObserver(cryptonote::ICryptonoteProtocolObserver* observer);
  virtual uint64_t getObservedHeight() const;
  virtual size_t getPeerCount() const;
  void setPeerCount(uint32_t count);
  void setObservedHeight(uint64_t height);

private:
  size_t peers;
  uint64_t observedHeight;
};

