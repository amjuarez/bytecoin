// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "JsonOutputStreamSerializer.h"
#include <cassert>
#include <stdexcept>
#include "Common/StringTools.h"

using Common::JsonValue;
using namespace CryptoNote;

namespace CryptoNote {
std::ostream& operator<<(std::ostream& out, const JsonOutputStreamSerializer& enumerator) {
  out << enumerator.root;
  return out;
}
}

namespace {

template <typename T>
void insertOrPush(JsonValue& js, Common::StringView name, const T& value) {
  if (js.isArray()) {
    js.pushBack(JsonValue(value));
  } else {
    js.insert(std::string(name), JsonValue(value));
  }
}

}

JsonOutputStreamSerializer::JsonOutputStreamSerializer() : root(JsonValue::OBJECT) {
  chain.push_back(&root);
}

JsonOutputStreamSerializer::~JsonOutputStreamSerializer() {
}

ISerializer::SerializerType JsonOutputStreamSerializer::type() const {
  return ISerializer::OUTPUT;
}

bool JsonOutputStreamSerializer::beginObject(Common::StringView name) {
  JsonValue& parent = *chain.back();
  JsonValue obj(JsonValue::OBJECT);

  if (parent.isObject()) {
    chain.push_back(&parent.insert(std::string(name), obj));
  } else {
    chain.push_back(&parent.pushBack(obj));
  }

  return true;
}

void JsonOutputStreamSerializer::endObject() {
  assert(!chain.empty());
  chain.pop_back();
}

bool JsonOutputStreamSerializer::beginArray(size_t& size, Common::StringView name) {
  JsonValue val(JsonValue::ARRAY);
  JsonValue& res = chain.back()->insert(std::string(name), val);
  chain.push_back(&res);
  return true;
}

void JsonOutputStreamSerializer::endArray() {
  assert(!chain.empty());
  chain.pop_back();
}

bool JsonOutputStreamSerializer::operator()(uint64_t& value, Common::StringView name) {
  int64_t v = static_cast<int64_t>(value);
  return operator()(v, name);
}

bool JsonOutputStreamSerializer::operator()(uint16_t& value, Common::StringView name) {
  uint64_t v = static_cast<uint64_t>(value);
  return operator()(v, name);
}

bool JsonOutputStreamSerializer::operator()(int16_t& value, Common::StringView name) {
  int64_t v = static_cast<int64_t>(value);
  return operator()(v, name);
}

bool JsonOutputStreamSerializer::operator()(uint32_t& value, Common::StringView name) {
  uint64_t v = static_cast<uint64_t>(value);
  return operator()(v, name);
}

bool JsonOutputStreamSerializer::operator()(int32_t& value, Common::StringView name) {
  int64_t v = static_cast<int64_t>(value);
  return operator()(v, name);
}

bool JsonOutputStreamSerializer::operator()(int64_t& value, Common::StringView name) {
  insertOrPush(*chain.back(), name, value);
  return true;
}

bool JsonOutputStreamSerializer::operator()(double& value, Common::StringView name) {
  insertOrPush(*chain.back(), name, value);
  return true;
}

bool JsonOutputStreamSerializer::operator()(std::string& value, Common::StringView name) {
  insertOrPush(*chain.back(), name, value);
  return true;
}

bool JsonOutputStreamSerializer::operator()(uint8_t& value, Common::StringView name) {
  insertOrPush(*chain.back(), name, static_cast<int64_t>(value));
  return true;
}

bool JsonOutputStreamSerializer::operator()(bool& value, Common::StringView name) {
  insertOrPush(*chain.back(), name, value);
  return true;
}

bool JsonOutputStreamSerializer::binary(void* value, size_t size, Common::StringView name) {
  std::string hex = Common::toHex(value, size);
  return (*this)(hex, name);
}

bool JsonOutputStreamSerializer::binary(std::string& value, Common::StringView name) {
  return binary(const_cast<char*>(value.data()), value.size(), name);
}
