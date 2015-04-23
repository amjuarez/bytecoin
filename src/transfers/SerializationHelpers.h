// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
