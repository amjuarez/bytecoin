// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ISerializer.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <cstring>

namespace cryptonote {

template<typename T>
typename std::enable_if<std::is_trivial<T>::value>::type
serializeAsBinary(std::vector<T>& value, const std::string& name, cryptonote::ISerializer& serializer) {
  std::string blob;
  if (serializer.type() == ISerializer::INPUT) {
    serializer.binary(blob, name);
    value.resize(blob.size() / sizeof(T));
    if (blob.size()) {
      memcpy(&value[0], blob.data(), blob.size());
    }
  } else {
    if (!value.empty()) {
      blob.assign(reinterpret_cast<const char*>(&value[0]), value.size() * sizeof(T));
    }
    serializer.binary(blob, name);
  }
}

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

template<typename K, typename V, typename Hash>
void serialize(std::unordered_map<K, V, Hash>& value, const std::string& name, cryptonote::ISerializer& serializer) {
  std::size_t size;
  size = value.size();

  serializer.beginArray(size, name);

  if (serializer.type() == cryptonote::ISerializer::INPUT) {
    value.reserve(size);

    for (size_t i = 0; i < size; ++i) {
      K key;
      V v;
      serializer.beginObject("");
      serializer(key, "key");
      serializer(v, "value");
      serializer.endObject();

      value[key] = v;
    }
  } else {
    for (auto kv: value) {
      K key;
      key = kv.first;
      serializer.beginObject("");
      serializer(key, "key");
      serializer(kv.second, "value");
      serializer.endObject();
    }
  }

  serializer.endArray();
}

template<std::size_t size>
void serialize(std::array<uint8_t, size>& value, const std::string& name, cryptonote::ISerializer& s) {
  s.binary(value.data(), value.size(), name);
}

}
