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
#include <memory>
#include "serialization.h"

template <template <bool> class Archive>
inline bool do_serialize(Archive<false>& ar, std::string& str)
{
  size_t size = 0;
  ar.serialize_varint(size);
  if (ar.remaining_bytes() < size)
  {
    ar.stream().setstate(std::ios::failbit);
    return false;
  }

  std::unique_ptr<std::string::value_type[]> buf(new std::string::value_type[size]);
  ar.serialize_blob(buf.get(), size);
  str.erase();
  str.append(buf.get(), size);
  return true;
}


template <template <bool> class Archive>
inline bool do_serialize(Archive<true>& ar, std::string& str)
{
  size_t size = str.size();
  ar.serialize_varint(size);
  ar.serialize_blob(const_cast<std::string::value_type*>(str.c_str()), size);
  return true;
}
