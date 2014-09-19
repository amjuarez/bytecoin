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

#include "serialization.h"

namespace serialization
{
  namespace detail
  {
    template <typename Archive, class T>
    bool serialize_vector_element(Archive& ar, T& e)
    {
      return ::do_serialize(ar, e);
    }

    template <typename Archive>
    bool serialize_vector_element(Archive& ar, uint32_t& e)
    {
      ar.serialize_varint(e);
      return true;
    }

    template <typename Archive>
    bool serialize_vector_element(Archive& ar, uint64_t& e)
    {
      ar.serialize_varint(e);
      return true;
    }
  }
}

template <template <bool> class Archive, class T>
bool do_serialize(Archive<false> &ar, std::vector<T> &v)
{
  size_t cnt;
  ar.begin_array(cnt);
  if (!ar.stream().good())
    return false;
  v.clear();

  // very basic sanity check
  if (ar.remaining_bytes() < cnt) {
    ar.stream().setstate(std::ios::failbit);
    return false;
  }

  v.reserve(cnt);
  for (size_t i = 0; i < cnt; i++) {
    if (i > 0)
      ar.delimit_array();
    v.resize(i+1);
    if (!::serialization::detail::serialize_vector_element(ar, v[i]))
      return false;
    if (!ar.stream().good())
      return false;
  }
  ar.end_array();
  return true;
}

template <template <bool> class Archive, class T>
bool do_serialize(Archive<true> &ar, std::vector<T> &v)
{
  size_t cnt = v.size();
  ar.begin_array(cnt);
  for (size_t i = 0; i < cnt; i++) {
    if (!ar.stream().good())
      return false;
    if (i > 0)
      ar.delimit_array();
    if(!::serialization::detail::serialize_vector_element(ar, v[i]))
      return false;
    if (!ar.stream().good())
      return false;
  }
  ar.end_array();
  return true;
}
