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

template<typename T>
typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, void>::type
writeVarint(std::ostream& s, T i) {
  while (i >= 0x80) {
    s.put((static_cast<char>(i)& 0x7f) | 0x80);
    i >>= 7;
  }
  s.put(static_cast<char>(i));

  if (!s) {
    throw std::runtime_error("Stream write error");
  }
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
  writeVarint(stream, size);
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::endArray() {
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(uint8_t& value, const std::string& name) {
  writeVarint(stream, value);
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(uint32_t& value, const std::string& name) {
  writeVarint(stream, value);
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(int32_t& value, const std::string& name) {
  writeVarint(stream, static_cast<uint32_t>(value));
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(int64_t& value, const std::string& name) {
  writeVarint(stream, static_cast<uint64_t>(value));
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(uint64_t& value, const std::string& name) {
  writeVarint(stream, value);
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(bool& value, const std::string& name) {
  char boolVal = value;
  checkedWrite(&boolVal, 1);
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::operator()(std::string& value, const std::string& name) {
  writeVarint(stream, value.size());
  checkedWrite(value.data(), value.size());
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::binary(void* value, std::size_t size, const std::string& name) {
  checkedWrite(static_cast<const char*>(value), size);
  return *this;
}

ISerializer& BinaryOutputStreamSerializer::binary(std::string& value, const std::string& name) {
  // write as string (with size prefix)
  return (*this)(value, name);
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

void BinaryOutputStreamSerializer::checkedWrite(const char* buf, size_t size) {
  stream.write(buf, size);
  if (!stream) {
    throw std::runtime_error("Stream write error");
  }
}

}
