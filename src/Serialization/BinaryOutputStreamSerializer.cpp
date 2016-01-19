// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "BinaryOutputStreamSerializer.h"

#include <cassert>
#include <stdexcept>
#include "Common/StreamTools.h"

using namespace Common;

namespace CryptoNote {

ISerializer::SerializerType BinaryOutputStreamSerializer::type() const {
  return ISerializer::OUTPUT;
}

bool BinaryOutputStreamSerializer::beginObject(Common::StringView name) {
  return true;
}

void BinaryOutputStreamSerializer::endObject() {
}

bool BinaryOutputStreamSerializer::beginArray(size_t& size, Common::StringView name) {
  writeVarint(stream, size);
  return true;
}

void BinaryOutputStreamSerializer::endArray() {
}

bool BinaryOutputStreamSerializer::operator()(uint8_t& value, Common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(uint16_t& value, Common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(int16_t& value, Common::StringView name) {
  writeVarint(stream, static_cast<uint16_t>(value));
  return true;
}

bool BinaryOutputStreamSerializer::operator()(uint32_t& value, Common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(int32_t& value, Common::StringView name) {
  writeVarint(stream, static_cast<uint32_t>(value));
  return true;
}

bool BinaryOutputStreamSerializer::operator()(int64_t& value, Common::StringView name) {
  writeVarint(stream, static_cast<uint64_t>(value));
  return true;
}

bool BinaryOutputStreamSerializer::operator()(uint64_t& value, Common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(bool& value, Common::StringView name) {
  char boolVal = value;
  checkedWrite(&boolVal, 1);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(std::string& value, Common::StringView name) {
  writeVarint(stream, value.size());
  checkedWrite(value.data(), value.size());
  return true;
}

bool BinaryOutputStreamSerializer::binary(void* value, size_t size, Common::StringView name) {
  checkedWrite(static_cast<const char*>(value), size);
  return true;
}

bool BinaryOutputStreamSerializer::binary(std::string& value, Common::StringView name) {
  // write as string (with size prefix)
  return (*this)(value, name);
}

bool BinaryOutputStreamSerializer::operator()(double& value, Common::StringView name) {
  assert(false); //the method is not supported for this type of serialization
  throw std::runtime_error("double serialization is not supported in BinaryOutputStreamSerializer");
  return false;
}

void BinaryOutputStreamSerializer::checkedWrite(const char* buf, size_t size) {
  write(stream, buf, size);
}

}
