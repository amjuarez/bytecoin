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

#include <istream>
#include <memory>
#include "../Common/JsonValue.h"
#include "ISerializer.h"
#include "JsonInputValueSerializer.h"
#include "SerializationOverloads.h"

namespace CryptoNote {

class KVBinaryInputStreamSerializer : public JsonInputValueSerializer {
public:
  KVBinaryInputStreamSerializer(std::istream& strm) : stream(strm) {}
  virtual ~KVBinaryInputStreamSerializer() {}

  void parse();

  virtual ISerializer& binary(void* value, std::size_t size, const std::string& name) override;
  virtual ISerializer& binary(std::string& value, const std::string& name) override;

private:
  std::unique_ptr<Common::JsonValue> root;
  std::istream& stream;

  Common::JsonValue loadSection();
  Common::JsonValue loadEntry();
  Common::JsonValue loadValue(uint8_t type);
  Common::JsonValue loadArray(uint8_t itemType);
};

}
