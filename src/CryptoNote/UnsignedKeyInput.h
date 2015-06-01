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

class UnsignedKeyInput {
public:
  UnsignedKeyInput(uint64_t amount, std::vector<uint32_t>&& outputs, const crypto::key_image& keyImage);
  UnsignedKeyInput(const UnsignedKeyInput& other) = delete;
  UnsignedKeyInput& operator=(const UnsignedKeyInput& other) = delete;
  uint64_t getAmount() const;
  uint32_t getOutputCount() const;
  uint32_t getOutputIndex(uint32_t index) const;
  const crypto::key_image& getKeyImage() const;

private:
  uint64_t amount;
  std::vector<uint32_t> outputs;
  crypto::key_image keyImage;
};

}
