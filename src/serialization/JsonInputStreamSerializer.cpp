// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "serialization/JsonInputStreamSerializer.h"

#include <ctype.h>
#include <exception>

namespace cryptonote {

JsonInputStreamSerializer::JsonInputStreamSerializer(std::istream& stream) {
  stream >> root;
  JsonInputValueSerializer::setJsonValue(&root);
}

JsonInputStreamSerializer::~JsonInputStreamSerializer() {
}

} //namespace cryptonote
