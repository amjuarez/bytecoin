// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <sstream>
#include <type_traits>
#include <boost/tti/has_member_function.hpp>

#include "serialization/JsonOutputStreamSerializer.h"
#include "serialization/JsonInputStreamSerializer.h"
#include "storages/portable_storage_template_helper.h"

namespace {
BOOST_TTI_HAS_MEMBER_FUNCTION(serialize)
} //namespace

namespace cryptonote {

template<class T>
inline typename std::enable_if<has_member_function_serialize<void (T::*)(ISerializer&, const std::string&)>::value, void>::type SerializeToJson(T& obj, std::string& jsonBuff) {
  std::stringstream stream;
  JsonOutputStreamSerializer serializer;

  obj.serialize(serializer, "");

  stream << serializer;
  jsonBuff = stream.str();
}

template<class T>
inline typename std::enable_if<has_member_function_serialize<void (T::*)(ISerializer&, const std::string&)>::value, void>::type LoadFromJson(T& obj, const std::string& jsonBuff) {
  std::stringstream stream(jsonBuff);
  JsonInputStreamSerializer serializer(stream);

  obj.serialize(serializer, "");
}

//old epee serialization

template<class T>
inline typename std::enable_if<!has_member_function_serialize<void (T::*)(ISerializer&, const std::string&)>::value, void>::type SerializeToJson(T& obj, std::string& jsonBuff) {
  epee::serialization::store_t_to_json(obj, jsonBuff);
}

template<class T>
inline typename std::enable_if<!has_member_function_serialize<void (T::*)(ISerializer&, const std::string&)>::value, void>::type LoadFromJson(T& obj, const std::string& jsonBuff) {
  epee::serialization::load_t_from_json(obj, jsonBuff);
}

} //namespace cryptonote
