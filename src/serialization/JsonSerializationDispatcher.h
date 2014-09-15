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
