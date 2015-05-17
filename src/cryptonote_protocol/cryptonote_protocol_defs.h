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

#include <list>
#include "serialization/keyvalue_serialization.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_protocol/blobdatatype.h"
namespace cryptonote
{


#define BC_COMMANDS_POOL_BASE 2000


  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct block_complete_entry
  {
    blobdata block;
    std::list<blobdata> txs;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(block)
      KV_SERIALIZE(txs)
    END_KV_SERIALIZE_MAP()
  };

  struct BlockFullInfo : public block_complete_entry
  {
    crypto::hash block_id;

    BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE_VAL_POD_AS_BLOB(block_id)
    KV_SERIALIZE(block)
    KV_SERIALIZE(txs)
    END_KV_SERIALIZE_MAP()
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_NEW_BLOCK_request
  {
    block_complete_entry b;
    uint64_t current_blockchain_height;
    uint32_t hop;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(b)
      KV_SERIALIZE(current_blockchain_height)
      KV_SERIALIZE(hop)
    END_KV_SERIALIZE_MAP()
  };

  struct NOTIFY_NEW_BLOCK
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 1;
    typedef NOTIFY_NEW_BLOCK_request request;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_NEW_TRANSACTIONS_request
  {
    std::list<blobdata>   txs;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(txs)
    END_KV_SERIALIZE_MAP()
  };

  struct NOTIFY_NEW_TRANSACTIONS
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 2;
    typedef NOTIFY_NEW_TRANSACTIONS_request request;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_REQUEST_GET_OBJECTS_request
  {
    std::list<crypto::hash>    txs;
    std::list<crypto::hash>    blocks;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE_CONTAINER_POD_AS_BLOB(txs)
      KV_SERIALIZE_CONTAINER_POD_AS_BLOB(blocks)
    END_KV_SERIALIZE_MAP()
  };

  struct NOTIFY_REQUEST_GET_OBJECTS
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 3;
    typedef NOTIFY_REQUEST_GET_OBJECTS_request request;
  };

  struct NOTIFY_RESPONSE_GET_OBJECTS_request
  {
    std::list<blobdata>              txs;
    std::list<block_complete_entry>  blocks;
    std::list<crypto::hash>               missed_ids;
    uint64_t                         current_blockchain_height;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(txs)
      KV_SERIALIZE(blocks)
      KV_SERIALIZE_CONTAINER_POD_AS_BLOB(missed_ids)
      KV_SERIALIZE(current_blockchain_height)
    END_KV_SERIALIZE_MAP()
  };

  struct NOTIFY_RESPONSE_GET_OBJECTS
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 4;
    typedef NOTIFY_RESPONSE_GET_OBJECTS_request request;
  };


  struct CORE_SYNC_DATA
  {
    uint64_t current_height;
    crypto::hash  top_id;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(current_height)
      KV_SERIALIZE_VAL_POD_AS_BLOB(top_id)
    END_KV_SERIALIZE_MAP()
  };

  struct NOTIFY_REQUEST_CHAIN
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 6;

    struct request
    {
      std::list<crypto::hash> block_ids; /*IDs of the first 10 blocks are sequential, next goes with pow(2,n) offset, like 2, 4, 8, 16, 32, 64 and so on, and the last one is always genesis block */

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(block_ids)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct NOTIFY_RESPONSE_CHAIN_ENTRY_request
  {
    uint64_t start_height;
    uint64_t total_height;
    std::list<crypto::hash> m_block_ids;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(start_height)
      KV_SERIALIZE(total_height)
      KV_SERIALIZE_CONTAINER_POD_AS_BLOB(m_block_ids)
    END_KV_SERIALIZE_MAP()
  };

  struct NOTIFY_RESPONSE_CHAIN_ENTRY
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 7;
    typedef NOTIFY_RESPONSE_CHAIN_ENTRY_request request;
  };

}
