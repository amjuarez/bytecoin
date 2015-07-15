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

#include "KVBinaryInputStreamSerializer.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include "KVBinaryCommon.h"

using Common::JsonValue;
using namespace CryptoNote;

namespace {

void strictRead(std::istream& s, void* ptr, size_t size) {
  if (s.read(reinterpret_cast<char*>(ptr), size).gcount() != size) {
    throw std::runtime_error("read error");
  }
}

template <typename T>
T readPod(std::istream& s) {
  T v;
  strictRead(s, &v, sizeof(T));
  return v;
}

template <typename T, typename JsonT = T>
JsonValue readPodJson(std::istream& s) {
  JsonValue jv;
  jv = static_cast<JsonT>(readPod<T>(s));
  return jv;
}

template <typename T>
JsonValue readIntegerJson(std::istream& s) {
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
  if (size) {
    strictRead(s, &str[0], size);
  }
  return str;
}

JsonValue readStringJson(std::istream& s) {
  return JsonValue(readString(s));
}

void readName(std::istream& s, std::string& name) {
  uint8_t len = readPod<uint8_t>(s);
  if (len) {
    name.resize(len);
    strictRead(s, &name[0], len);
  }
}

JsonValue loadValue(std::istream& stream, uint8_t type);
JsonValue loadSection(std::istream& stream);
JsonValue loadEntry(std::istream& stream);
JsonValue loadArray(std::istream& stream, uint8_t itemType);


JsonValue loadSection(std::istream& stream) {
  JsonValue sec(JsonValue::OBJECT);
  size_t count = readVarint(stream);
  std::string name;

  while (count--) {
    readName(stream, name);
    sec.insert(name, loadEntry(stream));
  }

  return sec;
}

JsonValue loadValue(std::istream& stream, uint8_t type) {
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
  case BIN_KV_SERIALIZE_TYPE_BOOL:   return JsonValue(stream.get() != 0);
  case BIN_KV_SERIALIZE_TYPE_STRING: return readStringJson(stream);
  case BIN_KV_SERIALIZE_TYPE_OBJECT: return loadSection(stream);
  case BIN_KV_SERIALIZE_TYPE_ARRAY:  return loadArray(stream, type);
  default:
    throw std::runtime_error("Unknown data type");
    break;
  }
}

JsonValue loadEntry(std::istream& stream) {
  uint8_t type = readPod<uint8_t>(stream);

  if (type & BIN_KV_SERIALIZE_FLAG_ARRAY) {
    type &= ~BIN_KV_SERIALIZE_FLAG_ARRAY;
    return loadArray(stream, type);
  }

  return loadValue(stream, type);
}

JsonValue loadArray(std::istream& stream, uint8_t itemType) {
  JsonValue arr(JsonValue::ARRAY);
  size_t count = readVarint(stream);

  while (count--) {
    arr.pushBack(loadValue(stream, itemType));
  }

  return arr;
}


JsonValue parseBinary(std::istream& stream) {
  auto hdr = readPod<KVBinaryStorageBlockHeader>(stream);

  if (
    hdr.m_signature_a != PORTABLE_STORAGE_SIGNATUREA ||
    hdr.m_signature_b != PORTABLE_STORAGE_SIGNATUREB) {
    throw std::runtime_error("Invalid binary storage signature");
  }

  if (hdr.m_ver != PORTABLE_STORAGE_FORMAT_VER) {
    throw std::runtime_error("Unknown binary storage format version");
  }

  return loadSection(stream);
}

}

KVBinaryInputStreamSerializer::KVBinaryInputStreamSerializer(std::istream& strm) : value(parseBinary(strm)), JsonInputValueSerializer(value) {
}

bool KVBinaryInputStreamSerializer::binary(void* value, std::size_t size, Common::StringView name) {
  std::string str;

  if (!(*this)(str, name)) {
    return false;
  }

  if (str.size() != size) {
    throw std::runtime_error("Binary block size mismatch");
  }

  memcpy(value, str.data(), size);
  return true;
}

bool KVBinaryInputStreamSerializer::binary(std::string& value, Common::StringView name) {
  return (*this)(value, name); // load as string
}

