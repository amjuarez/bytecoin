// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>

#include "../../src/serialization/ISerializer.h"
#include "../../src/rpc/core_rpc_server_commands_defs.h"

namespace cryptonote {

void serialize(COMMAND_RPC_START_MINING::request& value, const std::string& name, cryptonote::ISerializer& serializer);
void serialize(COMMAND_RPC_START_MINING::response& value, const std::string& name, cryptonote::ISerializer& serializer);

void serialize(COMMAND_RPC_STOP_MINING::request& value, const std::string& name, cryptonote::ISerializer& serializer);
void serialize(COMMAND_RPC_STOP_MINING::response& value, const std::string& name, cryptonote::ISerializer& serializer);

void serialize(COMMAND_RPC_STOP_DAEMON::request& value, const std::string& name, cryptonote::ISerializer& serializer);
void serialize(COMMAND_RPC_STOP_DAEMON::response& value, const std::string& name, cryptonote::ISerializer& serializer);

} //namespace cryptonote
