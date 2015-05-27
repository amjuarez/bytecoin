// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

JsonOutputStreamSerializer::JsonOutputStreamSerializer() : root(JsonValue::OBJECT) {
}

JsonOutputStreamSerializer::~JsonOutputStreamSerializer() {
}

JsonValue JsonOutputStreamSerializer::getJsonValue() const {
  return root;
}

ISerializer::SerializerType JsonOutputStreamSerializer::type() const {
  return ISerializer::OUTPUT;
}

ISerializer& JsonOutputStreamSerializer::beginObject(const std::string& name) {
  if (chain.size() == 0) {
    chain.push_back(&root);
    return *this;
  }

  JsonValue* parent = chain.back();
  JsonValue obj(JsonValue::OBJECT);

  if (parent->isObject()) {
    JsonValue& res = parent->insert(name, obj);
    chain.push_back(&res);
  } else {
    JsonValue& res = parent->pushBack(obj);
    chain.push_back(&res);
  }

  return *this;
}

ISerializer& JsonOutputStreamSerializer::endObject() {
  chain.pop_back();
  return *this;
}

ISerializer& JsonOutputStreamSerializer::beginArray(std::size_t& size, const std::string& name) {
  JsonValue val(JsonValue::ARRAY);
  JsonValue& res = chain.back()->insert(name, val);
  chain.push_back(&res);
  return *this;
}

ISerializer& JsonOutputStreamSerializer::endArray() {
  chain.pop_back();
  return *this;
}

ISerializer& JsonOutputStreamSerializer::operator()(uint64_t& value, const std::string& name) {
  int64_t v = static_cast<int64_t>(value);
  return operator()(v, name);
}

ISerializer& JsonOutputStreamSerializer::operator()(uint32_t& value, const std::string& name) {
  uint64_t v = static_cast<uint64_t>(value);
  return operator()(v, name);
}

ISerializer& JsonOutputStreamSerializer::operator()(int32_t& value, const std::string& name) {
  int64_t v = static_cast<int64_t>(value);
  return operator()(v, name);
}

ISerializer& JsonOutputStreamSerializer::operator()(int64_t& value, const std::string& name) {
  JsonValue* val = chain.back();
  JsonValue v;
  v = static_cast<int64_t>(value);
  if (val->isArray()) {
    val->pushBack(v);
  } else {
    val->insert(name, v);
  }
  return *this;
}

ISerializer& JsonOutputStreamSerializer::operator()(double& value, const std::string& name) {
  JsonValue* val = chain.back();
  JsonValue v;
  v = static_cast<double>(value);

  if (val->isArray()) {
    val->pushBack(v);
  } else {
    val->insert(name, v);
  }
  return *this;
}

ISerializer& JsonOutputStreamSerializer::operator()(std::string& value, const std::string& name) {
  JsonValue* val = chain.back();
  JsonValue v;
  v = value;

  if (val->isArray()) {
    val->pushBack(v);
  } else {
    val->insert(name, v);
  }
  return *this;
}

ISerializer& JsonOutputStreamSerializer::operator()(uint8_t& value, const std::string& name) {
  uint64_t v = static_cast<uint64_t>(value);
  return operator()(v, name);
}

ISerializer& JsonOutputStreamSerializer::operator()(bool& value, const std::string& name) {
  JsonValue* val = chain.back();
  if (val->isArray()) {
    val->pushBack(JsonValue(value));
  } else {
    val->insert(name, JsonValue(value));
  }

  return *this;
}

ISerializer& JsonOutputStreamSerializer::binary(void* value, std::size_t size, const std::string& name) {
  auto hex = Common::toHex(value, size);
  return (*this)(hex, name);
}

ISerializer& JsonOutputStreamSerializer::binary(std::string& value, const std::string& name) {
  return binary(const_cast<char*>(value.data()), value.size(), name);
}

bool JsonOutputStreamSerializer::hasObject(const std::string& name) {
  assert(false);
  throw std::runtime_error("JsonOutputStreamSerializer doesn't support this type of serialization");

  return false;
}
