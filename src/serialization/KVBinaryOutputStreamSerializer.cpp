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

#include "KVBinaryOutputStreamSerializer.h"
#include "KVBinaryCommon.h"

#include <cassert>
#include <stdexcept>

using namespace CryptoNote;
using namespace cryptonote;

namespace {

template <typename T>
void writePod(IOutputStream& s, const T& value) {
  s.write((const char*)&value, sizeof(T));
}

template<class T>
size_t packVarint(IOutputStream& s, uint8_t type_or, size_t pv) {
  T v = static_cast<T>(pv << 2);
  v |= type_or;
  s.write((const char*)&v, sizeof(T));
  return sizeof(T);
}

void writeElementName(IOutputStream& s, const std::string& name) {
  if (name.size() > std::numeric_limits<uint8_t>::max()) {
    throw std::runtime_error("Element name is too long");
  }
  uint8_t len = static_cast<uint8_t>(name.size());
  s.write((const char*)&len, sizeof(len));
  s.write(name.data(), len);
}

size_t writeArraySize(IOutputStream& s, size_t val) {
  if (val <= 63) {
    return packVarint<uint8_t>(s, PORTABLE_RAW_SIZE_MARK_BYTE, val);
  } else if (val <= 16383) {
    return packVarint<uint16_t>(s, PORTABLE_RAW_SIZE_MARK_WORD, val);
  } else if (val <= 1073741823) {
    return packVarint<uint32_t>(s, PORTABLE_RAW_SIZE_MARK_DWORD, val);
  } else {
    if (val > 4611686018427387903) {
      throw std::runtime_error("failed to pack varint - too big amount");
    }
    return packVarint<uint64_t>(s, PORTABLE_RAW_SIZE_MARK_INT64, val);
  }
}

}

namespace cryptonote {

using namespace CryptoNote;




KVBinaryOutputStreamSerializer::KVBinaryOutputStreamSerializer() {
  beginObject(std::string());
}

void KVBinaryOutputStreamSerializer::write(std::ostream& target) {

  assert(m_objectsStack.size() == 1);
  assert(m_stack.size() == 1);

  KVBinaryStorageBlockHeader hdr;
  hdr.m_signature_a = PORTABLE_STORAGE_SIGNATUREA;
  hdr.m_signature_b = PORTABLE_STORAGE_SIGNATUREB;
  hdr.m_ver = PORTABLE_STORAGE_FORMAT_VER;

  target.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
  target.write(stream().data(), stream().size());
}

ISerializer::SerializerType KVBinaryOutputStreamSerializer::type() const {
  return ISerializer::OUTPUT;
}

ISerializer& KVBinaryOutputStreamSerializer::beginObject(const std::string& name) {
  checkArrayPreamble(BIN_KV_SERIALIZE_TYPE_OBJECT);
 
  m_stack.push_back(Level(name));
  m_objectsStack.push_back(MemoryStream());

  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::endObject() {
  assert(m_objectsStack.size());

  auto level = std::move(m_stack.back());
  m_stack.pop_back();

  auto objStream = std::move(m_objectsStack.back());
  m_objectsStack.pop_back();

  auto& out = stream();

  writeElementPrefix(BIN_KV_SERIALIZE_TYPE_OBJECT, level.name);

  writeArraySize(out, level.count);
  out.write(objStream.data(), objStream.size());

  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::beginArray(std::size_t& size, const std::string& name) {
  m_stack.push_back(Level(name, size));
  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::endArray() {
  bool validArray = m_stack.back().state == State::Array;
  m_stack.pop_back();

  if (m_stack.back().state == State::Object && validArray) {
    ++m_stack.back().count;
  }

  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::operator()(uint8_t& value, const std::string& name) {
  writeElementPrefix(BIN_KV_SERIALIZE_TYPE_UINT8, name);
  writePod(stream(), value);
  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::operator()(uint32_t& value, const std::string& name) {
  writeElementPrefix(BIN_KV_SERIALIZE_TYPE_UINT32, name);
  writePod(stream(), value);
  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::operator()(int32_t& value, const std::string& name) {
  writeElementPrefix(BIN_KV_SERIALIZE_TYPE_INT32, name);
  writePod(stream(), value);
  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::operator()(int64_t& value, const std::string& name) {
  writeElementPrefix(BIN_KV_SERIALIZE_TYPE_INT64, name);
  writePod(stream(), value);
  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::operator()(uint64_t& value, const std::string& name) {
  writeElementPrefix(BIN_KV_SERIALIZE_TYPE_UINT64, name);
  writePod(stream(), value);
  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::operator()(bool& value, const std::string& name) {
  writeElementPrefix(BIN_KV_SERIALIZE_TYPE_BOOL, name);
  writePod(stream(), value);
  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::operator()(double& value, const std::string& name) {
  writeElementPrefix(BIN_KV_SERIALIZE_TYPE_DOUBLE, name);
  writePod(stream(), value);
  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::operator()(std::string& value, const std::string& name) {
  writeElementPrefix(BIN_KV_SERIALIZE_TYPE_STRING, name);

  auto& out = stream();
  writeArraySize(out, value.size());
  out.write(value.data(), value.size());
  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::binary(void* value, std::size_t size, const std::string& name) {
  if (size > 0) {
    writeElementPrefix(BIN_KV_SERIALIZE_TYPE_STRING, name);
    auto& out = stream();
    writeArraySize(out, size);
    out.write(static_cast<const char*>(value), size);
  }
  return *this;
}

ISerializer& KVBinaryOutputStreamSerializer::binary(std::string& value, const std::string& name) {
  return binary(const_cast<char*>(value.data()), value.size(), name);
}

bool KVBinaryOutputStreamSerializer::hasObject(const std::string& name) {
  assert(false); //the method is not supported for this type of serialization
  throw std::runtime_error("hasObject method is not supported in KVBinaryOutputStreamSerializer");

  return false;
}

void KVBinaryOutputStreamSerializer::writeElementPrefix(uint8_t type, const std::string& name) {  
  assert(m_stack.size());

  checkArrayPreamble(type);
  Level& level = m_stack.back();
  
  if (level.state != State::Array) {
    if (!name.empty()) {
      auto& s = stream();
      writeElementName(s, name);
      s.write((const char*)&type, 1);
    }
    ++level.count;
  }
}

void KVBinaryOutputStreamSerializer::checkArrayPreamble(uint8_t type) {
  if (m_stack.empty()) {
    return;
  }

  Level& level = m_stack.back();

  if (level.state == State::ArrayPrefix) {
    auto& s = stream();
    writeElementName(s, level.name);
    char c = BIN_KV_SERIALIZE_FLAG_ARRAY | type;
    s.write(&c, 1);
    writeArraySize(s, level.count);
    level.state = State::Array;
  }
}


MemoryStream& KVBinaryOutputStreamSerializer::stream() {
  assert(m_objectsStack.size());
  return m_objectsStack.back();
}

}
