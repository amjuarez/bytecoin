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

#pragma once

#include "../crypto/crypto.h"

namespace CryptoNote {

class KeyOutput {
public:
  KeyOutput(uint64_t amount, const crypto::public_key& key);
  KeyOutput(const KeyOutput& other) = delete;
  KeyOutput& operator=(const KeyOutput& other) = delete;
  uint64_t getAmount() const;
  const crypto::public_key& getKey() const;

private:
  uint64_t amount;
  crypto::public_key key;
};

}
