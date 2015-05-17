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

#include "p2p/net_node_common.h"
#include "cryptonote_core/connection_context.h"

namespace cryptonote
{
  struct NOTIFY_NEW_BLOCK_request;
  struct NOTIFY_NEW_TRANSACTIONS_request;

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct i_cryptonote_protocol {
    virtual void relay_block(NOTIFY_NEW_BLOCK_request& arg, cryptonote_connection_context& exclude_context)=0;
    virtual void relay_transactions(NOTIFY_NEW_TRANSACTIONS_request& arg, cryptonote_connection_context& exclude_context)=0;
    //virtual bool request_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, cryptonote_connection_context& context)=0;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct cryptonote_protocol_stub: public i_cryptonote_protocol {
    virtual void relay_block(NOTIFY_NEW_BLOCK_request& arg, cryptonote_connection_context& exclude_context) override {
    }

    virtual void relay_transactions(NOTIFY_NEW_TRANSACTIONS_request& arg, cryptonote_connection_context& exclude_context) override {
    }
  };
}
