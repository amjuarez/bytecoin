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

#include "crypto/crypto.h"
#include "cryptonote_config.h"
#include "cryptonote_core/cryptonote_stat_info.h"

// epee
#include "serialization/keyvalue_serialization.h"

namespace CryptoNote
{
  struct network_config
  {
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(connections_count)
      KV_SERIALIZE(handshake_interval)
      KV_SERIALIZE(packet_max_size)
      KV_SERIALIZE(config_id)
    END_KV_SERIALIZE_MAP()

    uint32_t connections_count;
    uint32_t connection_timeout;
    uint32_t ping_connection_timeout;
    uint32_t handshake_interval;
    uint32_t packet_max_size;
    uint32_t config_id;
    uint32_t send_peerlist_sz;
  };

  struct basic_node_data
  {
    uuid network_id;                   
    uint64_t local_time;
    uint32_t my_port;
    peerid_type peer_id;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE_VAL_POD_AS_BLOB(network_id)
      KV_SERIALIZE(peer_id)
      KV_SERIALIZE(local_time)
      KV_SERIALIZE(my_port)
    END_KV_SERIALIZE_MAP()
  };
  
  struct CORE_SYNC_DATA
  {
    uint64_t current_height;
    crypto::hash top_id;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(current_height)
      KV_SERIALIZE_VAL_POD_AS_BLOB(top_id)
    END_KV_SERIALIZE_MAP()
  };

#define P2P_COMMANDS_POOL_BASE 1000

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct COMMAND_HANDSHAKE
  {
    const static int ID = P2P_COMMANDS_POOL_BASE + 1;

    struct request
    {
      basic_node_data node_data;
      CORE_SYNC_DATA payload_data;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(node_data)
        KV_SERIALIZE(payload_data)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      basic_node_data node_data;
      CORE_SYNC_DATA payload_data;
      std::list<peerlist_entry> local_peerlist; 

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(node_data)
        KV_SERIALIZE(payload_data)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(local_peerlist)
      END_KV_SERIALIZE_MAP()
    };
  };


  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct COMMAND_TIMED_SYNC
  {
    const static int ID = P2P_COMMANDS_POOL_BASE + 2;

    struct request
    {
      CORE_SYNC_DATA payload_data;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(payload_data)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      uint64_t local_time;
      CORE_SYNC_DATA payload_data;
      std::list<peerlist_entry> local_peerlist; 

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(local_time)
        KV_SERIALIZE(payload_data)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(local_peerlist)
      END_KV_SERIALIZE_MAP()
    };
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/

  struct COMMAND_PING
  {
    /*
      Used to make "callback" connection, to be sure that opponent node 
      have accessible connection point. Only other nodes can add peer to peerlist,
      and ONLY in case when peer has accepted connection and answered to ping.
    */
    const static int ID = P2P_COMMANDS_POOL_BASE + 3;

#define PING_OK_RESPONSE_STATUS_TEXT "OK"

    struct request
    {
      /*actually we don't need to send any real data*/

      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status;
      peerid_type peer_id;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(peer_id)
      END_KV_SERIALIZE_MAP()    
    };
  };

  
#ifdef ALLOW_DEBUG_COMMANDS
  //These commands are considered as insecure, and made in debug purposes for a limited lifetime. 
  //Anyone who feel unsafe with this commands can disable the ALLOW_GET_STAT_COMMAND macro.

  struct proof_of_trust
  {
    peerid_type peer_id;
    uint64_t    time;
    crypto::signature sign;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(peer_id)
      KV_SERIALIZE(time)        
      KV_SERIALIZE_VAL_POD_AS_BLOB(sign)  
    END_KV_SERIALIZE_MAP()    
  };

  inline crypto::hash get_proof_of_trust_hash(const proof_of_trust& pot) {
    std::string s;
    s.append(reinterpret_cast<const char*>(&pot.peer_id), sizeof(pot.peer_id));
    s.append(reinterpret_cast<const char*>(&pot.time), sizeof(pot.time));
    return crypto::cn_fast_hash(s.data(), s.size());
  }

  struct COMMAND_REQUEST_STAT_INFO
  {
    const static int ID = P2P_COMMANDS_POOL_BASE + 4;

    struct request
    {
      proof_of_trust tr;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(tr)
      END_KV_SERIALIZE_MAP()    
    };
    
    struct response
    {
      std::string version;
      std::string os_version;
      uint64_t connections_count;
      uint64_t incoming_connections_count;
      core_stat_info payload_info;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version)
        KV_SERIALIZE(os_version)
        KV_SERIALIZE(connections_count)
        KV_SERIALIZE(incoming_connections_count)
        KV_SERIALIZE(payload_info)
      END_KV_SERIALIZE_MAP()    
    };
  };


  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct COMMAND_REQUEST_NETWORK_STATE
  {
    const static int ID = P2P_COMMANDS_POOL_BASE + 5;

    struct request
    {
      proof_of_trust tr;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(tr)
      END_KV_SERIALIZE_MAP()    
    };

    struct response
    {
      std::list<peerlist_entry> local_peerlist_white; 
      std::list<peerlist_entry> local_peerlist_gray; 
      std::list<connection_entry> connections_list; 
      peerid_type my_id;
      uint64_t    local_time;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(local_peerlist_white)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(local_peerlist_gray)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(connections_list)
        KV_SERIALIZE(my_id)
        KV_SERIALIZE(local_time)
      END_KV_SERIALIZE_MAP()    
    };
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct COMMAND_REQUEST_PEER_ID
  {
    const static int ID = P2P_COMMANDS_POOL_BASE + 6;

    struct request
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()    
    };

    struct response
    {
      peerid_type my_id;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(my_id)
      END_KV_SERIALIZE_MAP()    
    };
  };

#endif


}
