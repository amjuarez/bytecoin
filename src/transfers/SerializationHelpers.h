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

#include "serialization/ISerializer.h"

namespace cryptonote {

template <typename Element, typename Iterator>
void writeSequence(Iterator begin, Iterator end, const std::string& name, ISerializer& s) {
  size_t size = std::distance(begin, end);
  s.beginArray(size, name);
  for (Iterator i = begin; i != end; ++i) {
    s(const_cast<Element&>(*i), "");
  }
  s.endArray();
}

template <typename Element, typename Iterator>
void readSequence(Iterator outputIterator, const std::string& name, ISerializer& s) {
  size_t size = 0;
  s.beginArray(size, name);

  while (size--) {
    Element e;
    s(e, "");
    *outputIterator++ = e;
  }

  s.endArray();
}

}
