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

#include "BinaryOutputStreamSerializer.h"

#include <cassert>
#include <stdexcept>

namespace {

void serializeInteger(std::ostream& stream, uint8_t v) {
  stream.put(static_cast<char>(v));
}

void serializeBool(std::ostream& stream, bool v) {
  serializeInteger(stream, static_cast<uint8_t>(v));
}

void serializeInteger(std::ostream& stream, uint32_t v) {
  stream.put(static_cast<char>(v & 0xff));
  v >>= 8;

  stream.put(static_cast<char>(v & 0xff));
  v >>= 8;

  stream.put(static_cast<char>(v & 0xff));
  v >>= 8;

  stream.put(static_cast<char>(v & 0xff));
}

void serializeInteger(std::ostream& stream, uint64_t v) {
  stream.put(static_cast<unsigned char>(v & 0xff));
  v >>= 8;

  stream.put(static_cast<unsigned char>(v & 0xff));
  v >>= 8;

  stream.put(static_cast<unsigned char>(v & 0xff));
  v >>= 8;

  stream.put(static_cast<unsigned char>(v & 0xff));
  v >>= 8;

  stream.put(static_cast<unsigned char>(v & 0xff));
  v >>= 8;

  stream.put(static_cast<unsigned char>(v & 0xff));
  v >>= 8;

  stream.put(static_cast<unsigned char>(v & 0xff));
  v >>= 8;

  stream.put(static_cast<unsigned char>(v & 0xff));
}

void serializeInteger(std::ostream& stream, int64_t v) {
  serializeInteger(stream, static_cast<uint64_t>(v));
}

void serializeData(std::ostream& stream, const char* buf, size_t len) {
  stream.write(buf, len);
}

}

namespace cryptonote {

ISerializer::SerializerType BinaryOutputStreamSerializer::type() const {
  return ISerializer::OUTPUT;
}

ISerializer& BinaryOutputStreamSerializer::beginObject(const std::string& name) {
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::endObject() {
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::beginArray(std::size_t& size, const std::string& name) {
  uint64_t size64 = size;
  serializeVarint(size64, name, *this);
  size = size64;
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::endArray() {
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(uint8_t& value, const std::string& name) {
  serializeInteger(stream, value);

  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(uint32_t& value, const std::string& name) {
  serializeInteger(stream, value);

  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(int32_t& value, const std::string& name) {
  uint32_t v = value;
  operator()(v, name);

  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(int64_t& value, const std::string& name) {
  serializeInteger(stream, value);

  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(uint64_t& value, const std::string& name) {
  serializeInteger(stream, value);

  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(bool& value, const std::string& name) {
  serializeBool(stream, value);

  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(std::string& value, const std::string& name) {
  uint64_t size = value.size();
  serializeVarint(size, name, *this);
  serializeData(stream, value.c_str(), value.size());

  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(char* value, std::size_t size, const std::string& name) {
  serializeData(stream, value, size);

  return *this;
}

ISerializer& BinaryOutputStreamSerializer::tag(const std::string& name) {
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::untagged(uint8_t& value) {
  stream.put(value);

  return *this;
}

ISerializer& BinaryOutputStreamSerializer::endTag() {
  return *this;
}

bool BinaryOutputStreamSerializer::hasObject(const std::string& name) {
  assert(false); //the method is not supported for this type of serialization
  throw std::runtime_error("hasObject method is not supported in BinaryOutputStreamSerializer");

  return false;
}

ISerializer& BinaryOutputStreamSerializer::operator()(double& value, const std::string& name) {
  assert(false); //the method is not supported for this type of serialization
  throw std::runtime_error("double serialization is not supported in BinaryOutputStreamSerializer");

  return *this;
}

}
