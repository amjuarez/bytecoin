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

#include "serialization/ISerializer.h"
#include "serialization/JsonValue.h"

#include <iostream>

namespace cryptonote {

class JsonOutputStreamSerializer : public ISerializer {
public:
  JsonOutputStreamSerializer();
  virtual ~JsonOutputStreamSerializer();

  JsonValue getJsonValue() const;
  SerializerType type() const;

  virtual ISerializer& beginObject(const std::string& name) override;
  virtual ISerializer& endObject() override;

  virtual ISerializer& beginArray(std::size_t& size, const std::string& name) override;
  virtual ISerializer& endArray() override;

  virtual ISerializer& operator()(int32_t& value, const std::string& name) override;
  virtual ISerializer& operator()(uint32_t& value, const std::string& name) override;
  virtual ISerializer& operator()(int64_t& value, const std::string& name) override;
  virtual ISerializer& operator()(uint64_t& value, const std::string& name) override;
  virtual ISerializer& operator()(double& value, const std::string& name) override;
  virtual ISerializer& operator()(std::string& value, const std::string& name) override;
  virtual ISerializer& operator()(uint8_t& value, const std::string& name) override;
  virtual ISerializer& operator()(bool& value, const std::string& name) override;
  
  virtual ISerializer& binary(void* value, std::size_t size, const std::string& name) override;
  virtual ISerializer& binary(std::string& value, const std::string& name) override;

  virtual bool hasObject(const std::string& name) override;

  template<typename T>
  ISerializer& operator()(T& value, const std::string& name) {
    return ISerializer::operator()(value, name);
  }

  friend std::ostream& operator<<(std::ostream& out, const JsonOutputStreamSerializer& enumerator);

private:
  JsonValue root;
  std::vector<JsonValue*> chain;
};

} // namespace cryptonote
