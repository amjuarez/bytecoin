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
#include <list>
#include <unordered_set>

#include "net/net_utils_base.h"
#include "copyable_atomic.h"

#include "crypto/hash.h"

namespace cryptonote
{

  struct cryptonote_connection_context: public epee::net_utils::connection_context_base
  {

    enum state
    {
      state_befor_handshake = 0, //default state
      state_synchronizing,
      state_idle,
      state_normal
    };

    state m_state;
    std::list<crypto::hash> m_needed_objects;
    std::unordered_set<crypto::hash> m_requested_objects;
    uint64_t m_remote_blockchain_height;
    uint64_t m_last_response_height;
    epee::copyable_atomic m_callback_request_count; //in debug purpose: problem with double callback rise
    //size_t m_score;  TODO: add score calculations
  };

  inline std::string get_protocol_state_string(cryptonote_connection_context::state s)
  {
    switch (s)
    {
    case cryptonote_connection_context::state_befor_handshake:
      return "state_befor_handshake";
    case cryptonote_connection_context::state_synchronizing:
      return "state_synchronizing";
    case cryptonote_connection_context::state_idle:
      return "state_idle";
    case cryptonote_connection_context::state_normal:
      return "state_normal";
    default:
      return "unknown";
    }    
  }

}
