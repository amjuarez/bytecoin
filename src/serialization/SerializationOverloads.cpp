// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "SerializationOverloads.h"

#include <limits>

namespace cryptonote {

//void readVarint(uint64_t& value, cryptonote::ISerializer& serializer) {
//  const int bits = std::numeric_limits<uint64_t>::digits;
//
//  uint64_t v = 0;
//  for (int shift = 0;; shift += 7) {
//    uint8_t b;
//    serializer.untagged(b);
//
//    if (shift + 7 >= bits && b >= 1 << (bits - shift)) {
//      throw std::runtime_error("Varint overflow");
//    }
//
//    if (b == 0 && shift != 0) {
//      throw std::runtime_error("Non-canonical varint representation");
//    }
//
//    v |= static_cast<uint64_t>(b & 0x7f) << shift;
//    if ((b & 0x80) == 0) {
//      break;
//    }
//  }
//
//  value = v;
//}
//
//void writeVarint(uint64_t& value, cryptonote::ISerializer& serializer) {
//  uint64_t v = value;
//
//  while (v >= 0x80) {
//    uint8_t b = (static_cast<uint8_t>(v) & 0x7f) | 0x80;
//    serializer.untagged(b);
//    v >>= 7;
//  }
//
//  uint8_t b = static_cast<uint8_t>(v);
//  serializer.untagged(b);
//}
//
//
//void serializeVarint(uint64_t& value, const std::string& name, cryptonote::ISerializer& serializer) {
//  serializer.tag(name);
//
//  if (serializer.type() == cryptonote::ISerializer::INPUT) {
//    readVarint(value, serializer);
//  } else {
//    writeVarint(value, serializer);
//  }
//
//  serializer.endTag();
//}
//
//void serializeVarint(uint32_t& value, const std::string& name, cryptonote::ISerializer& serializer) {
//  uint64_t v = value;
//  serializeVarint(v, name, serializer);
//  value = static_cast<uint32_t>(v);
//}

}
