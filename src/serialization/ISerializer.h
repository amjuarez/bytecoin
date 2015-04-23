// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
  virtual ISerializer& operator()(std::string& value, const std::string& name) = 0;
  
  // read/write binary block
  virtual ISerializer& binary(void* value, std::size_t size, const std::string& name) = 0;
  virtual ISerializer& binary(std::string& value, const std::string& name) = 0;

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

}
