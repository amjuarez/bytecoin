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

#include <string>
#include <cstdint>

namespace cryptonote {

class ISerializer {
public:

  enum SerializerType {
    INPUT,
    OUTPUT
  };

  virtual ~ISerializer() {}

  virtual SerializerType type() const = 0;

  virtual ISerializer& beginObject(const std::string& name) = 0;
  virtual ISerializer& endObject() = 0;
  virtual ISerializer& beginArray(std::size_t& size, const std::string& name) = 0;
  virtual ISerializer& endArray() = 0;

  virtual ISerializer& operator()(uint8_t& value, const std::string& name) = 0;
  virtual ISerializer& operator()(int32_t& value, const std::string& name) = 0;
  virtual ISerializer& operator()(uint32_t& value, const std::string& name) = 0;
  virtual ISerializer& operator()(int64_t& value, const std::string& name) = 0;
  virtual ISerializer& operator()(uint64_t& value, const std::string& name) = 0;
  virtual ISerializer& operator()(double& value, const std::string& name) = 0;
  virtual ISerializer& operator()(bool& value, const std::string& name) = 0;
  virtual ISerializer& operator()(char* value, std::size_t size, const std::string& name) = 0;
  virtual ISerializer& operator()(std::string& value, const std::string& name) = 0;

  virtual ISerializer& tag(const std::string& name) = 0;
  virtual ISerializer& untagged(uint8_t& value) = 0;
  virtual ISerializer& endTag() = 0;

  virtual bool hasObject(const std::string& name) = 0;

  template<typename T>
  ISerializer& operator()(T& value, const std::string& name);
};

template<typename T>
ISerializer& ISerializer::operator()(T& value, const std::string& name) {
  serialize(value, name, *this);
  return *this;
}

template<typename T>
void serialize(T& value, const std::string& name, ISerializer& serializer) {
  value.serialize(serializer, name);
  return;
}

/*
template<typename T>
void serialize(std::vector<T>& value, const std::string& name);

template<typename K, typename V>
void serialize(std::unordered_map<K, V>& value, const std::string& name);



template<typename T>
void ISerializer::serialize(std::vector<T>& value, const std::string& name) {
  std::size_t size = value.size();
  beginArray(size, name);
  value.resize(size);

  for (size_t i = 0; i < size; ++i) {
    serialize(value[i], "");
  }

  endArray();

  return *this;
}

template<typename K, typename V>
void ISerializer::serialize(std::unordered_map<K, V>& value, const std::string& name) {
  std::size_t size;
  size = value.size();

  beginArray(size, name);

  if (type() == INPUT) {
    value.reserve(size);

    for (size_t i = 0; i < size; ++i) {
      K key;
      V v;
      beginObject("");
      serialize(key, "");
      serialize(v, "");
      endObject();

      value[key] = v;
    }
  } else {
    for (auto kv: value) {
      K key;
      key = kv.first;
      beginObject("");
      serialize(key, "");
      serialize(kv.second, "");
      endObject();
    }
  }

  endArray();

  return *this;
}
*/

}
