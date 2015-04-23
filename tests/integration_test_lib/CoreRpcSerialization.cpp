// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CoreRpcSerialization.h"

namespace cryptonote {

void serialize(COMMAND_RPC_START_MINING::request& value, const std::string& name, cryptonote::ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(value.miner_address, "miner_address");
  serializer(value.threads_count, "threads_count");
  serializer.endObject();
}

void serialize(COMMAND_RPC_START_MINING::response& value, const std::string& name, cryptonote::ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(value.status, "status");
  serializer.endObject();
}

void serialize(COMMAND_RPC_STOP_MINING::request& value, const std::string& name, cryptonote::ISerializer& serializer) {
  serializer.beginObject(name);
  serializer.endObject();
}

void serialize(COMMAND_RPC_STOP_MINING::response& value, const std::string& name, cryptonote::ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(value.status, "status");
  serializer.endObject();
}

void serialize(COMMAND_RPC_STOP_DAEMON::request& value, const std::string& name, cryptonote::ISerializer& serializer) {
  serializer.beginObject(name);
  serializer.endObject();
}

void serialize(COMMAND_RPC_STOP_DAEMON::response& value, const std::string& name, cryptonote::ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(value.status, "status");
  serializer.endObject();
}

} //namespace cryptonote
