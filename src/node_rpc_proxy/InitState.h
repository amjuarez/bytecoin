// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <atomic>

#include "include_base_utils.h"

namespace tools {

class InitState {
public:
  InitState() : m_state(STATE_NOT_INITIALIZED) {
  }

  bool initialized() const volatile {
    return STATE_INITIALIZED == m_state.load(std::memory_order_acquire);
  }

  bool beginInit() volatile {
    State state = STATE_NOT_INITIALIZED;
    if (!m_state.compare_exchange_strong(state, STATE_INITIALIZING, std::memory_order_seq_cst)) {
      LOG_ERROR("object has been already initialized");
      return false;
    }
    return true;
  }

  bool endInit() volatile {
    State expectedState = STATE_INITIALIZING;
    if (!m_state.compare_exchange_strong(expectedState, STATE_INITIALIZED, std::memory_order_seq_cst)) {
      LOG_ERROR("Unexpected state: " << expectedState);
      return false;
    }
    return true;
  }

  bool beginShutdown() volatile {
    while (true) {
      State state = m_state.load(std::memory_order_relaxed);
      if (STATE_NOT_INITIALIZED == state) {
        return true;
      } else if (STATE_INITIALIZING == state) {
        LOG_ERROR("Object is being initialized");
        return false;
      } else if (STATE_INITIALIZED == state) {
        if (m_state.compare_exchange_strong(state, STATE_SHUTTING_DOWN, std::memory_order_seq_cst)) {
          return true;
        }
      } else if (STATE_SHUTTING_DOWN == state) {
        LOG_ERROR("Object is being shutting down");
        return false;
      } else {
        LOG_ERROR("Unknown state " << state);
        return false;
      }
    }
  }

  bool endShutdown() volatile {
    State expectedState = STATE_SHUTTING_DOWN;
    if (!m_state.compare_exchange_strong(expectedState, STATE_NOT_INITIALIZED, std::memory_order_seq_cst)) {
      LOG_ERROR("Unexpected state: " << expectedState);
      return false;
    }
    return true;
  }

private:
  enum State {
    STATE_NOT_INITIALIZED,
    STATE_INITIALIZING,
    STATE_INITIALIZED,
    STATE_SHUTTING_DOWN
  };

private:
  std::atomic<State> m_state;
};

}
