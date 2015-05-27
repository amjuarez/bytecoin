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

#include "MultisignatureOutput.h"
#include <cassert>

namespace CryptoNote {

MultisignatureOutput::MultisignatureOutput(uint64_t amount, std::vector<crypto::public_key>&& keys, uint32_t requiredSignatureCount) : amount(amount), keys(std::move(keys)), requiredSignatureCount(requiredSignatureCount) {
  assert(requiredSignatureCount <= keys.size());
}

uint64_t MultisignatureOutput::getAmount() const {
  return amount;
}

uint32_t MultisignatureOutput::getKeyCount() const {
  return static_cast<uint32_t>(keys.size());
}

const crypto::public_key& MultisignatureOutput::getKey(uint32_t index) const {
  return keys[index];
}

uint32_t MultisignatureOutput::getRequiredSignatureCount() const {
  return requiredSignatureCount;
}

}
