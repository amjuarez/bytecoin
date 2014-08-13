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
