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

#include "ISerializer.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace cryptonote {

void serializeVarint(uint64_t& value, const std::string& name, cryptonote::ISerializer& serializer);
void serializeVarint(uint32_t& value, const std::string& name, cryptonote::ISerializer& serializer);

template<typename T>
void serialize(std::vector<T>& value, const std::string& name, cryptonote::ISerializer& serializer) {
  std::size_t size = value.size();
  serializer.beginArray(size, name);
  value.resize(size);

  for (size_t i = 0; i < size; ++i) {
    serializer(value[i], "");
  }

  serializer.endArray();
}

template<typename K, typename V>
void serialize(std::unordered_map<K, V>& value, const std::string& name, cryptonote::ISerializer& serializer) {
  std::size_t size;
  size = value.size();

  serializer.beginArray(size, name);

  if (serializer.type() == cryptonote::ISerializer::INPUT) {
    value.reserve(size);

    for (size_t i = 0; i < size; ++i) {
      K key;
      V v;
      serializer.beginObject("");
      serializer(key, "");
      serializer(v, "");
      serializer.endObject();

      value[key] = v;
    }
  } else {
    for (auto kv: value) {
      K key;
      key = kv.first;
      serializer.beginObject("");
      serializer(key, "");
      serializer(kv.second, "");
      serializer.endObject();
    }
  }

  serializer.endArray();
}

}
