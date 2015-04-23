// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

  if (parent->count(name)) {
    const JsonValue& arr = (*parent)(name);
    size = arr.size();
    chain.push_back(&arr);
  } else {
    size = 0;
    chain.push_back(0);
  }

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
  value = static_cast<uint32_t>(getNumber(name));
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(int32_t& value, const std::string& name) {
  assert(root);
  value = static_cast<int32_t>(getNumber(name));
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(int64_t& value, const std::string& name) {
  assert(root);
  value = getNumber(name);
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(uint64_t& value, const std::string& name) {
  assert(root);
  value = static_cast<uint64_t>(getNumber(name));
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(double& value, const std::string& name) {
  assert(root);
  value = getValue(name).getDouble();
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(std::string& value, const std::string& name) {
  assert(root);
  value = getValue(name).getString();
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(uint8_t& value, const std::string& name) {
  assert(root);
  value = static_cast<uint8_t>(getNumber(name));
  return *this;
}

ISerializer& JsonInputValueSerializer::operator()(bool& value, const std::string& name) {
  assert(root);
  value = getValue(name).getBool();
  return *this;
}

bool JsonInputValueSerializer::hasObject(const std::string& name) {
  return chain.back()->count(name) != 0;
}

ISerializer& JsonInputValueSerializer::binary(void* value, std::size_t size, const std::string& name) {
  assert(false);
  throw std::runtime_error("JsonInputValueSerializer doesn't support this type of serialization");

  return *this;
}

ISerializer& JsonInputValueSerializer::binary(std::string& value, const std::string& name) {
  assert(false);
  throw std::runtime_error("JsonInputValueSerializer doesn't support this type of serialization");

  return *this;
}

JsonValue JsonInputValueSerializer::getValue(const std::string& name) {
  const JsonValue& val = *chain.back();
  return val.isArray() ? val[idxs.back()++] : val(name);
}

int64_t JsonInputValueSerializer::getNumber(const std::string& name) {
  return getValue(name).getNumber();
}


}
