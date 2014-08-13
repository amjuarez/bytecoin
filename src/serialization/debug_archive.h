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

#include "json_archive.h"
#include "variant.h"

template <bool W>
struct debug_archive : public json_archive<W> {
  typedef typename json_archive<W>::stream_type stream_type;

  debug_archive(stream_type &s) : json_archive<W>(s) { }
};

template <class T>
struct serializer<debug_archive<true>, T>
{
  static void serialize(debug_archive<true> &ar, T &v)
  {
    ar.begin_object();
    ar.tag(variant_serialization_traits<debug_archive<true>, T>::get_tag());
    serializer<json_archive<true>, T>::serialize(ar, v);
    ar.end_object();
    ar.stream() << std::endl;
  }
};
