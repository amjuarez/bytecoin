// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ContextGroupTimeout.h"
#include <System/InterruptedException.h>

using namespace System;

ContextGroupTimeout::ContextGroupTimeout(Dispatcher& dispatcher, ContextGroup& contextGroup, std::chrono::nanoseconds timeout) :
  workingContextGroup(dispatcher),
  timeoutTimer(dispatcher) {
  workingContextGroup.spawn([&, timeout] {
    try {
      timeoutTimer.sleep(timeout);
      contextGroup.interrupt();
    } catch (InterruptedException&) {
    }
  });
}
