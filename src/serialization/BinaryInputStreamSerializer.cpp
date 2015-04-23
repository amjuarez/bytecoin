// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "BinaryInputStreamSerializer.h"
#include "SerializationOverloads.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace {

template<typename T, int bits = std::numeric_limits<T>::digits>
typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, size_t>::type
readVarint(std::istream& s, T &i) {
  size_t read = 0;
  i = 0;
  for (int shift = 0;; shift += 7) {
    if (s.eof()) {
      return read;
    }
    uint8_t byte = s.get();
    if (!s) {
      throw std::runtime_error("Stream read error");
    }
    ++read;
    if (shift + 7 >= bits && byte >= 1 << (bits - shift)) {
      throw std::runtime_error("Varint overflow");
    }
    if (byte == 0 && shift != 0) {
      throw std::runtime_error("Non-canonical varint representation");
    }
    i |= static_cast<T>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      break;
    }
  }
  return read;
}

template<typename StorageType, typename T>
void readVarintAs(std::istream& s, T &i) {
  StorageType v;
  readVarint(s, v);
  i = static_cast<T>(v);
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
  readVarintAs<uint64_t>(stream, size);
  return *this;
}

ISerializer& BinaryInputStreamSerializer::endArray() {
  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(uint8_t& value, const std::string& name) {
  readVarint(stream, value);
  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(uint32_t& value, const std::string& name) {
  readVarint(stream, value);
  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(int32_t& value, const std::string& name) {
  readVarintAs<uint32_t>(stream, value);
  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(int64_t& value, const std::string& name) {
  readVarintAs<uint64_t>(stream, value);
  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(uint64_t& value, const std::string& name) {
  readVarint(stream, value);
  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(bool& value, const std::string& name) {
  value = static_cast<bool>(stream.get());
  return *this;
}

ISerializer& BinaryInputStreamSerializer::operator()(std::string& value, const std::string& name) {
  uint64_t size;
  readVarint(stream, size);

  if (size > 0) {
    std::vector<char> temp;
    temp.resize(size);
    checkedRead(&temp[0], size);
    value.reserve(size);
    value.assign(&temp[0], size);
  } else {
    value.clear();
  }

  return *this;
}

ISerializer& BinaryInputStreamSerializer::binary(void* value, std::size_t size, const std::string& name) {
  stream.read(static_cast<char*>(value), size);
  return *this;
}

ISerializer& BinaryInputStreamSerializer::binary(std::string& value, const std::string& name) {
  return (*this)(value, name);
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

void BinaryInputStreamSerializer::checkedRead(char* buf, size_t size) {
  stream.read(buf, size);
  if (!stream) {
    throw std::runtime_error("Stream read error");
  }
}


}
