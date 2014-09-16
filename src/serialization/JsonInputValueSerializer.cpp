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

#include "serialization/JsonInputValueSerializer.h"

#include <cassert>
#include <stdexcept>

namespace cryptonote {

JsonInputValueSerializer::JsonInputValueSerializer() : root(nullptr) {
}

JsonInputValueSerializer::~JsonInputValueSerializer() {
}

void JsonInputValueSerializer::setJsonValue(const JsonValue* value) {
  root = value;
}

ISerializer::SerializerType JsonInputValueSerializer::type() const {
  return ISerializer::INPUT;
}

ISerializer& JsonInputValueSerializer::beginObject(const std::string& name) {
  assert(root);

  if (chain.size() == 0) {
    chain.push_back(root);
    return *this;
  }

  const JsonValue* parent = chain.back();
  if (parent->isArray()) {
    const JsonValue& v = (*parent)[idxs.back()++];
    chain.push_back(&v);
  } else {
    const JsonValue& v = (*parent)(name);
    chain.push_back(&v);
  }

  return *this;
}

ISerializer& JsonInputValueSerializer::endObject() {
  assert(root);

  chain.pop_back();
  return *this;
}

ISerializer& JsonInputValueSerializer::beginArray(std::size_t& size, const std::string& name) {
  assert(root);

  const JsonValue* parent = chain.back();

  const JsonValue& arr = (*parent)(name);
  size = arr.size();

  chain.push_back(&arr);
  idxs.push_back(0);
  return *this;
}

ISerializer& JsonInputValueSerializer::endArray() {
  assert(root);

  chain.pop_back();
  idxs.pop_back();
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(uint32_t& value, const std::string& name) {
  assert(root);

  int64_t v = static_cast<int64_t>(value);
  operator()(v, name);
  value = static_cast<uint64_t>(v);
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(int32_t& value, const std::string& name) {
  assert(root);

  int64_t v = static_cast<int64_t>(value);
  operator()(v, name);
  value = static_cast<uint64_t>(v);
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(int64_t& value, const std::string& name) {
  assert(root);

  const JsonValue* val = chain.back();

  if (val->isArray()) {
    const JsonValue& v = (*val)[idxs.back()++];
    value = v.getNumber();
  } else {
    const JsonValue& v = (*val)(name);
    value = v.getNumber();
  }

  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(uint64_t& value, const std::string& name) {
  assert(root);

  int64_t v = static_cast<int64_t>(value);
  operator()(v, name);
  value = static_cast<uint64_t>(v);
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(double& value, const std::string& name) {
  assert(root);

  const JsonValue* val = chain.back();

  if (val->isArray()) {
    value = (*val)[idxs.back()++].getDouble();
  } else {
    value = (*val)(name).getDouble();
  }

  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(std::string& value, const std::string& name) {
  assert(root);

  const JsonValue* val = chain.back();

  if (val->isArray()) {
    value = (*val)[idxs.back()++].getString();
  } else {
    value = (*val)(name).getString();
  }

  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(uint8_t& value, const std::string& name) {
  assert(root);

  uint64_t v = static_cast<uint64_t>(value);
  operator()(v, name);
  value = static_cast<uint8_t>(value);

  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(bool& value, const std::string& name) {
  assert(root);

  const JsonValue* val = chain.back();

  if (val->isArray()) {
    value = (*val)[idxs.back()++].getBool();
  } else {
    value = (*val)(name).getBool();
  }

  return *this;
}

bool JsonInputValueSerializer::hasObject(const std::string& name) {
  const JsonValue* val = chain.back();

  return val->count(name) != 0;
}

ISerializer& JsonInputValueSerializer::operator()(char* value, std::size_t size, const std::string& name) {
  assert(false);
  throw std::runtime_error("JsonInputValueSerializer doesn't support this type of serialization");

  return *this;
}

ISerializer& JsonInputValueSerializer::tag(const std::string& name) {
  assert(false);
  throw std::runtime_error("JsonInputValueSerializer doesn't support this type of serialization");

  return *this;
}

ISerializer& JsonInputValueSerializer::untagged(uint8_t& value) {
  assert(false);
  throw std::runtime_error("JsonInputValueSerializer doesn't support this type of serialization");

  return *this;
}

ISerializer& JsonInputValueSerializer::endTag() {
  assert(false);
  throw std::runtime_error("JsonInputValueSerializer doesn't support this type of serialization");

  return *this;
}

}
