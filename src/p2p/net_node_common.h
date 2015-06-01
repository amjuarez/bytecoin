// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include "p2p_protocol_types.h"

namespace CryptoNote {

  struct cryptonote_connection_context;

  struct i_p2p_endpoint {
    virtual void relay_notify_to_all(int command, const std::string& data_buff, const net_connection_id* excludeConnection) = 0;
    virtual bool invoke_notify_to_peer(int command, const std::string& req_buff, const CryptoNote::cryptonote_connection_context& context) = 0;
    virtual uint64_t get_connections_count()=0;
    virtual void for_each_connection(std::function<void(CryptoNote::cryptonote_connection_context&, peerid_type)> f) = 0;
    // can be called from external threads
    virtual void externalRelayNotifyToAll(int command, const std::string& data_buff) = 0;
  };

  struct p2p_endpoint_stub: public i_p2p_endpoint {
    virtual void relay_notify_to_all(int command, const std::string& data_buff, const net_connection_id* excludeConnection) {}
    virtual bool invoke_notify_to_peer(int command, const std::string& req_buff, const CryptoNote::cryptonote_connection_context& context) { return true; }
    virtual void for_each_connection(std::function<void(CryptoNote::cryptonote_connection_context&, peerid_type)> f) {}
    virtual uint64_t get_connections_count() { return 0; }   
    virtual void externalRelayNotifyToAll(int command, const std::string& data_buff) {}
  };
}
