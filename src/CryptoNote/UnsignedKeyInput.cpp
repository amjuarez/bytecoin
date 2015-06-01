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

#include "UnsignedKeyInput.h"

namespace CryptoNote {

UnsignedKeyInput::UnsignedKeyInput(uint64_t amount, std::vector<uint32_t>&& outputs, const crypto::key_image& keyImage) : amount(amount), outputs(std::move(outputs)), keyImage(keyImage) {
}

uint64_t UnsignedKeyInput::getAmount() const {
  return amount;
}

uint32_t UnsignedKeyInput::getOutputCount() const {
  return static_cast<uint32_t>(outputs.size());
}

uint32_t UnsignedKeyInput::getOutputIndex(uint32_t index) const {
  return outputs[index];
}

const crypto::key_image& UnsignedKeyInput::getKeyImage() const {
  return keyImage;
}

}
