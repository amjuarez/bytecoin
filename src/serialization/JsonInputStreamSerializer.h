// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <iosfwd>
#include <string>
#include <vector>

//#include "serialization/Enumerator.h"
#include "serialization/JsonInputValueSerializer.h"
#include "serialization/JsonValue.h"

namespace cryptonote {

//deserialization
class JsonInputStreamSerializer : public JsonInputValueSerializer {
public:
  JsonInputStreamSerializer(std::istream& stream);
  virtual ~JsonInputStreamSerializer();

private:
  JsonValue root;
};

}
