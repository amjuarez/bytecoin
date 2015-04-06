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

#include <cstddef>

namespace cryptonote {
class ICryptonoteProtocolObserver;

class ICryptonoteProtocolQuery {
public:
  virtual bool addObserver(ICryptonoteProtocolObserver* observer) = 0;
  virtual bool removeObserver(ICryptonoteProtocolObserver* observer) = 0;

  virtual uint64_t getObservedHeight() const = 0;
  virtual size_t getPeerCount() const = 0;
};

} //namespace cryptonote
