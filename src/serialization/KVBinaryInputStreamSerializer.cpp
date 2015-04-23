// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "KVBinaryInputStreamSerializer.h"
#include "KVBinaryCommon.h"

#include "JsonValue.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <cstring>

using namespace CryptoNote;
using namespace cryptonote;

namespace {

template <typename T>
T readPod(std::istream& s) {
  T v;
  s.read(reinterpret_cast<char*>(&v), sizeof(T));
  return v;
}

template <typename T, typename JsonT = T>
cryptonote::JsonValue readPodJson(std::istream& s) {
  T v;
  s.read(reinterpret_cast<char*>(&v), sizeof(T));
  cryptonote::JsonValue jv;
  jv = static_cast<JsonT>(v);
  return jv;
}

template <typename T>
cryptonote::JsonValue readIntegerJson(std::istream& s) {
  return readPodJson<T, int64_t>(s);
}

size_t readVarint(std::istream& s) {
  size_t v = 0;
  uint8_t size_mask = uint8_t(s.peek()) & PORTABLE_RAW_SIZE_MARK_MASK;

  switch (size_mask) {
  case PORTABLE_RAW_SIZE_MARK_BYTE:
    v = readPod<uint8_t>(s);
    break;
  case PORTABLE_RAW_SIZE_MARK_WORD:
    v = readPod<uint16_t>(s);
    break;
  case PORTABLE_RAW_SIZE_MARK_DWORD:
    v = readPod<uint32_t>(s);
    break;
  case PORTABLE_RAW_SIZE_MARK_INT64:
    v = readPod<uint64_t>(s);
    break;
  default:
    throw std::runtime_error("unknown varint size_mask");
  }

  v >>= 2;
  return v;
}

std::string readString(std::istream& s) {
  auto size = readVarint(s);
  std::string str;
  str.resize(size);
  s.read(&str[0], size);
  return str;
}

JsonValue readStringJson(std::istream& s) {
  JsonValue js;
  js = readString(s);
  return js;
}

void readName(std::istream& s, std::string& name) {
  uint8_t len = readPod<uint8_t>(s);
  name.resize(len);
  s.read(&name[0], len);
}

}


namespace cryptonote {

void KVBinaryInputStreamSerializer::parse() {
  auto hdr = readPod<KVBinaryStorageBlockHeader>(stream);

  if (
    hdr.m_signature_a != PORTABLE_STORAGE_SIGNATUREA ||
    hdr.m_signature_b != PORTABLE_STORAGE_SIGNATUREB) {
    throw std::runtime_error("Invalid binary storage signature");
  }

  if (hdr.m_ver != PORTABLE_STORAGE_FORMAT_VER) {
    throw std::runtime_error("Unknown binary storage format version");
  }

  root.reset(new JsonValue(loadSection()));
  setJsonValue(root.get());
}

ISerializer& KVBinaryInputStreamSerializer::binary(void* value, std::size_t size, const std::string& name) {
  std::string str;

  (*this)(str, name);

  if (str.size() != size) {
    throw std::runtime_error("Binary block size mismatch");
  }

  memcpy(value, str.data(), size);
  return *this;
}

ISerializer& KVBinaryInputStreamSerializer::binary(std::string& value, const std::string& name) {
  if (!hasObject(name)) {
    value.clear();
    return *this;
  }

  return (*this)(value, name); // load as string
}

JsonValue KVBinaryInputStreamSerializer::loadSection() {
  JsonValue sec(JsonValue::OBJECT);
  size_t count = readVarint(stream);
  std::string name;

  while (count--) {
    readName(stream, name);
    sec.insert(name, loadEntry());
  }

  return sec;
}

JsonValue KVBinaryInputStreamSerializer::loadValue(uint8_t type) {
  switch (type) {
  case BIN_KV_SERIALIZE_TYPE_INT64:  return readIntegerJson<int64_t>(stream);
  case BIN_KV_SERIALIZE_TYPE_INT32:  return readIntegerJson<int32_t>(stream);
  case BIN_KV_SERIALIZE_TYPE_INT16:  return readIntegerJson<int16_t>(stream);
  case BIN_KV_SERIALIZE_TYPE_INT8:   return readIntegerJson<int8_t>(stream);
  case BIN_KV_SERIALIZE_TYPE_UINT64: return readIntegerJson<uint64_t>(stream);
  case BIN_KV_SERIALIZE_TYPE_UINT32: return readIntegerJson<uint32_t>(stream);
  case BIN_KV_SERIALIZE_TYPE_UINT16: return readIntegerJson<uint16_t>(stream);
  case BIN_KV_SERIALIZE_TYPE_UINT8:  return readIntegerJson<uint8_t>(stream);
  case BIN_KV_SERIALIZE_TYPE_DOUBLE: return readPodJson<double>(stream);
  case BIN_KV_SERIALIZE_TYPE_BOOL:   return readPodJson<uint8_t, bool>(stream);
  case BIN_KV_SERIALIZE_TYPE_STRING: return readStringJson(stream);
  case BIN_KV_SERIALIZE_TYPE_OBJECT: return loadSection();
  case BIN_KV_SERIALIZE_TYPE_ARRAY:  return loadArray(type);
  default:
    throw std::runtime_error("Unknown data type");
    break;
  }
}

JsonValue KVBinaryInputStreamSerializer::loadEntry() {
  uint8_t type = readPod<uint8_t>(stream);

  if (type & BIN_KV_SERIALIZE_FLAG_ARRAY) {
    type &= ~BIN_KV_SERIALIZE_FLAG_ARRAY;
    return loadArray(type);
  }

  return loadValue(type);
}

JsonValue KVBinaryInputStreamSerializer::loadArray(uint8_t itemType) {
  JsonValue arr(JsonValue::ARRAY);
  size_t count = readVarint(stream);

  while (count--) {
    arr.pushBack(loadValue(itemType));
  }

  return arr;
}


}
