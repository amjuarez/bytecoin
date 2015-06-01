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

#include "MultisignatureInput.h"

namespace CryptoNote {

MultisignatureInput::MultisignatureInput(uint64_t amount, uint32_t outputIndex, std::vector<crypto::signature>&& signatures) : amount(amount), outputIndex(outputIndex), signatures(std::move(signatures)) {
}

uint64_t MultisignatureInput::getAmount() const {
  return amount;
}

uint32_t MultisignatureInput::getOutputIndex() const {
  return outputIndex;
}

uint32_t MultisignatureInput::getSignatureCount() const {
  return static_cast<uint32_t>(signatures.size());
}

const crypto::signature& MultisignatureInput::getSignature(uint32_t index) const {
  return signatures[index];
}

}
