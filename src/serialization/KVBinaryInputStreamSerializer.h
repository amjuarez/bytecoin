// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ISerializer.h"
#include "SerializationOverloads.h"

#include "JsonValue.h"
#include "JsonInputValueSerializer.h"

#include <istream>
#include <memory>

namespace cryptonote {

class KVBinaryInputStreamSerializer : public JsonInputValueSerializer {
public:
  KVBinaryInputStreamSerializer(std::istream& strm) : stream(strm) {}
  virtual ~KVBinaryInputStreamSerializer() {}

  void parse();

  virtual ISerializer& binary(void* value, std::size_t size, const std::string& name) override;
  virtual ISerializer& binary(std::string& value, const std::string& name) override;

private:

  JsonValue loadSection();
  JsonValue loadEntry();
  JsonValue loadValue(uint8_t type);
  JsonValue loadArray(uint8_t itemType);

  std::unique_ptr<JsonValue> root;
  std::istream& stream;
};

}
