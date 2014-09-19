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
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "crypto/hash.h"
#include "net/rpc_method_name.h"

namespace mining
{
  //-----------------------------------------------
#define CORE_RPC_STATUS_OK   "OK"


  struct job_details
  {
    std::string blob;
    std::string target;
    std::string job_id;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(blob)
      KV_SERIALIZE(target)
      KV_SERIALIZE(job_id)
    END_KV_SERIALIZE_MAP()
  };


  struct COMMAND_RPC_LOGIN
  {
    RPC_METHOD_NAME("login");
    
    struct request
    {
      std::string login;
      std::string pass;
      std::string agent;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(login)
        KV_SERIALIZE(pass)
        KV_SERIALIZE(agent)
      END_KV_SERIALIZE_MAP()
    };


    struct response
    {
      std::string status;
      std::string id;
      job_details job;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(id)
        KV_SERIALIZE(job)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_GETJOB
  {
    RPC_METHOD_NAME("getjob");

    struct request
    {
      std::string id;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(id)
      END_KV_SERIALIZE_MAP()
    };

    typedef job_details response;
  };

  struct COMMAND_RPC_SUBMITSHARE
  {
    RPC_METHOD_NAME("submit");

    struct request
    {
      std::string id;
      std::string nonce;
      std::string result;
      std::string job_id;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(id)
        KV_SERIALIZE(nonce)
        KV_SERIALIZE(result)
        KV_SERIALIZE(job_id)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };
}
