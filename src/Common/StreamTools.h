// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace Common {

class IInputStream;
class IOutputStream;

void read(IInputStream& in, void* data, size_t size);
void read(IInputStream& in, int8_t& value);
void read(IInputStream& in, int16_t& value);
void read(IInputStream& in, int32_t& value);
void read(IInputStream& in, int64_t& value);
void read(IInputStream& in, uint8_t& value);
void read(IInputStream& in, uint16_t& value);
void read(IInputStream& in, uint32_t& value);
void read(IInputStream& in, uint64_t& value);
void read(IInputStream& in, std::vector<uint8_t>& data, size_t size);
void read(IInputStream& in, std::string& data, size_t size);
void readVarint(IInputStream& in, uint8_t& value);
void readVarint(IInputStream& in, uint16_t& value);
void readVarint(IInputStream& in, uint32_t& value);
void readVarint(IInputStream& in, uint64_t& value);

void write(IOutputStream& out, const void* data, size_t size);
void write(IOutputStream& out, int8_t value);
void write(IOutputStream& out, int16_t value);
void write(IOutputStream& out, int32_t value);
void write(IOutputStream& out, int64_t value);
void write(IOutputStream& out, uint8_t value);
void write(IOutputStream& out, uint16_t value);
void write(IOutputStream& out, uint32_t value);
void write(IOutputStream& out, uint64_t value);
void write(IOutputStream& out, const std::vector<uint8_t>& data);
void write(IOutputStream& out, const std::string& data);
void writeVarint(IOutputStream& out, uint64_t value);

template<typename T> T read(IInputStream& in) {
  T value;
  read(in, value);
  return value;
}

template<typename T> T read(IInputStream& in, size_t size) {
  T value;
  read(in, value, size);
  return value;
}

template<typename T> T readVarint(IInputStream& in) {
  T value;
  readVarint(in, value);
  return value;
}

};
