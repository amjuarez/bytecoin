// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
