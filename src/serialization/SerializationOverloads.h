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

#include "ISerializer.h"

#include <array>
#include <cstring>
#include <list>
#include <string>
#include <vector>
#include <unordered_map>

namespace CryptoNote {

template<typename T>
typename std::enable_if<std::is_pod<T>::value>::type
serializeAsBinary(std::vector<T>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
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
typename std::enable_if<std::is_pod<T>::value>::type
serializeAsBinary(std::list<T>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  std::string blob;
  if (serializer.type() == ISerializer::INPUT) {
    serializer.binary(blob, name);

    size_t count = blob.size() / sizeof(T);
    const T* ptr = reinterpret_cast<const T*>(blob.data());

    while (count--) {
      value.push_back(*ptr++);
    }
  } else {
    if (!value.empty()) {
      blob.resize(value.size() * sizeof(T));
      T* ptr = reinterpret_cast<T*>(&blob[0]);

      for (const auto& item : value) {
        *ptr++ = item;
      }
    }
    serializer.binary(blob, name);
  }
}

template <typename Cont>
bool serializeContainer(Cont& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  std::size_t size = value.size();
  if (!serializer.beginArray(size, name)) {
    value.clear();
    return false;
  }

  value.resize(size);

  for (auto& item : value) {
    serializer(item, "");
  }

  serializer.endArray();
  return true;
}

template<typename T>
bool serialize(std::vector<T>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeContainer(value, name, serializer);
}

template<typename T>
bool serialize(std::list<T>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeContainer(value, name, serializer);
}


template<typename K, typename V, typename Hash>
bool serialize(std::unordered_map<K, V, Hash>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  std::size_t size;
  size = value.size();

  if (!serializer.beginArray(size, name)) {
    value.clear();
    return false;
  }

  if (serializer.type() == CryptoNote::ISerializer::INPUT) {
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
  return true;
}

template<std::size_t size>
bool serialize(std::array<uint8_t, size>& value, Common::StringView name, CryptoNote::ISerializer& s) {
  return s.binary(value.data(), value.size(), name);
}

}
