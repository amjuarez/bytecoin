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

#include "BinaryInputStreamSerializer.h"
#include "SerializationOverloads.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace {

void deserialize(std::istream& stream, uint8_t& v) {
  char c;
  stream.get(c);
  v = static_cast<uint8_t>(c);
}

void deserialize(std::istream& stream, int8_t& v) {
  uint8_t val;
  deserialize(stream, val);
  v = val;
}

void deserialize(std::istream& stream, bool& v) {
  uint8_t val;
  deserialize(stream, val);

  v = val;
}

void deserialize(std::istream& stream, uint32_t& v) {
  char c;

  stream.get(c);
  v = static_cast<uint8_t>(c);

  stream.get(c);
  v += static_cast<uint8_t>(c) << 8;

  stream.get(c);
  v += static_cast<uint8_t>(c) << 16;

  stream.get(c);
  v += static_cast<uint8_t>(c) << 24;
}

void deserialize(std::istream& stream, int32_t& v) {
  uint32_t val;
  deserialize(stream, val);
  v = val;
}

void deserialize(std::istream& stream, uint64_t& v) {
  char c;
  uint64_t uc;

  stream.get(c);
  uc = static_cast<unsigned char>(c);
  v = uc;

  stream.get(c);
  uc = static_cast<unsigned char>(c);
  v += (uc << 8);

  stream.get(c);
  uc = static_cast<unsigned char>(c);
  v += (uc << 16);

  stream.get(c);
  uc = static_cast<unsigned char>(c);
  v += (uc << 24);

  stream.get(c);
  uc = static_cast<unsigned char>(c);
  v += (uc << 32);

  stream.get(c);
  uc = static_cast<unsigned char>(c);
  v += (uc << 40);

  stream.get(c);
  uc = static_cast<unsigned char>(c);
  v += (uc << 48);

  stream.get(c);
  uc = static_cast<unsigned char>(c);
  v += (uc << 56);
}

void deserialize(std::istream& stream, int64_t& v) {
  uint64_t val;
  deserialize(stream, val);
  v = val;
}

void deserialize(std::istream& stream, char* buf, size_t len) {
  const size_t chunk = 1000;

//  stream.read(buf, len);

//  looks redundant, but i had a bug with it
  while (len && stream) {
    size_t toRead = std::min(len, chunk);
    stream.read(buf, toRead);
    len -= toRead;
    buf += toRead;
  }
}

}

namespace cryptonote {

ISerializer::SerializerType BinaryInputStreamSerializer::type() const {
  return ISerializer::INPUT;
}

ISerializer& BinaryInputStreamSerializer::beginObject(const std::string& name) {
  return *this;
}

ISerializer& BinaryInputStreamSerializer::endObject() {
  return *this;
}

ISerializer& BinaryInputStreamSerializer::beginArray(std::size_t& size, const std::string& name) {
  uint64_t val;
  serializeVarint(val, name, *this);
  size = val;

  return *this;
}

ISerializer& BinaryInputStreamSerializer::endArray() {
  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(uint8_t& value, const std::string& name) {
  deserialize(stream, value);

  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(uint32_t& value, const std::string& name) {
  deserialize(stream, value);

  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(int32_t& value, const std::string& name) {
  uint32_t v;
  operator()(v, name);
  value = v;

  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(int64_t& value, const std::string& name) {
  deserialize(stream, value);

  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(uint64_t& value, const std::string& name) {
  deserialize(stream, value);

  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(bool& value, const std::string& name) {
  deserialize(stream, value);

  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(std::string& value, const std::string& name) {
  uint64_t size;
  serializeVarint(size, name, *this);

  std::vector<char> temp;
  temp.resize(size);

  deserialize(stream, &temp[0], size);

  value.reserve(size);
  value.assign(&temp[0], size);

  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(char* value, std::size_t size, const std::string& name) {
  stream.read(value, size);

  return *this;
}

ISerializer& BinaryInputStreamSerializer::tag(const std::string& name) {
  return *this;
}

ISerializer& BinaryInputStreamSerializer::untagged(uint8_t& value) {
  char v;
  stream.get(v);
  value = v;

  return *this;
}

ISerializer& BinaryInputStreamSerializer::endTag() {
  return *this;
}

bool BinaryInputStreamSerializer::hasObject(const std::string& name) {
  assert(false); //the method is not supported for this type of serialization
  throw std::runtime_error("hasObject method is not supported in BinaryInputStreamSerializer");

  return false;
}

ISerializer& BinaryInputStreamSerializer::operator()(double& value, const std::string& name) {
  assert(false); //the method is not supported for this type of serialization
  throw std::runtime_error("double serialization is not supported in BinaryInputStreamSerializer");

  return *this;
}

}
