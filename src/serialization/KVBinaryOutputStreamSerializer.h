// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ISerializer.h"
#include "SerializationOverloads.h"
#include "MemoryStream.h"

#include <ostream>
#include <vector>
#include <list>
#include <sstream>

namespace cryptonote {

class KVBinaryOutputStreamSerializer : public ISerializer {
public:

  KVBinaryOutputStreamSerializer();
  virtual ~KVBinaryOutputStreamSerializer() {}

  void write(std::ostream& target);

  virtual ISerializer::SerializerType type() const;

  virtual ISerializer& beginObject(const std::string& name) override;
  virtual ISerializer& endObject() override;

  virtual ISerializer& beginArray(std::size_t& size, const std::string& name) override;
  virtual ISerializer& endArray() override;

  virtual ISerializer& operator()(uint8_t& value, const std::string& name) override;
  virtual ISerializer& operator()(int32_t& value, const std::string& name) override;
  virtual ISerializer& operator()(uint32_t& value, const std::string& name) override;
  virtual ISerializer& operator()(int64_t& value, const std::string& name) override;
  virtual ISerializer& operator()(uint64_t& value, const std::string& name) override;
  virtual ISerializer& operator()(double& value, const std::string& name) override;
  virtual ISerializer& operator()(bool& value, const std::string& name) override;
  virtual ISerializer& operator()(std::string& value, const std::string& name) override;

  virtual ISerializer& binary(void* value, std::size_t size, const std::string& name) override;
  virtual ISerializer& binary(std::string& value, const std::string& name) override;

  virtual bool hasObject(const std::string& name) override;

  template<typename T>
  ISerializer& operator()(T& value, const std::string& name) {
    return ISerializer::operator()(value, name);
  }

private:

  void writeElementPrefix(uint8_t type, const std::string& name);
  void checkArrayPreamble(uint8_t type);
  void updateState(uint8_t type);
  MemoryStream& stream();

  enum class State {
    Root,
    Object,
    ArrayPrefix,
    Array
  };

  struct Level {
    State state;
    std::string name;
    size_t count;

    Level(const std::string& nm) : 
      name(nm), state(State::Object), count(0) {}

    Level(const std::string& nm, size_t arraySize) : 
      name(nm), state(State::ArrayPrefix), count(arraySize) {}

    Level(Level&& rv) {
      state = rv.state;
      name = std::move(rv.name);
      count = rv.count;
    }

  };

  std::vector<MemoryStream> m_objectsStack;
  std::vector<Level> m_stack;
};

}
