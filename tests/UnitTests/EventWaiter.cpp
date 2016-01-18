// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "EventWaiter.h"

void EventWaiter::notify() {
  std::unique_lock<std::mutex> lck(mtx);
  available = true;
  cv.notify_all();
}

void EventWaiter::wait() {
  std::unique_lock<std::mutex> lck(mtx);
  cv.wait(lck, [this]() { return available; });
  available = false;
}

bool EventWaiter::wait_for(const std::chrono::milliseconds& rel_time) {
  std::unique_lock<std::mutex> lck(mtx);
  auto result = cv.wait_for(lck, rel_time, [this]() { return available; });
  available = false;
  return result;
}
