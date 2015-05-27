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

#include "BaseTransaction.h"
#include <algorithm>
#include <cassert>

namespace CryptoNote {

BaseTransaction::BaseTransaction(
  uint64_t blockIndex,
  uint64_t unlockTime,
  std::vector<KeyOutputEntry>&& keyOutputs,
  std::vector<MultisignatureOutputEntry>&& multisignatureOutputs,
  std::vector<uint8_t>&& extra) :
  blockIndex(blockIndex),
  unlockTime(unlockTime),
  keyOutputs(std::move(keyOutputs)),
  multisignatureOutputs(std::move(multisignatureOutputs)),
  extra(std::move(extra)) {
}

BaseTransaction::BaseTransaction(BaseTransaction&& other) : blockIndex(other.blockIndex), unlockTime(other.unlockTime), keyOutputs(std::move(other.keyOutputs)), multisignatureOutputs(std::move(other.multisignatureOutputs)), extra(std::move(other.extra)) {
}

uint64_t BaseTransaction::getBlockIndex() const {
  return blockIndex;
}

uint64_t BaseTransaction::getUnlockTime() const {
  return unlockTime;
}

uint32_t BaseTransaction::getOutputCount() const {
  return static_cast<uint32_t>(keyOutputs.size() + multisignatureOutputs.size());
}

BaseTransaction::OutputType BaseTransaction::getOutputType(uint32_t index) const {
  auto iterator = std::lower_bound(keyOutputs.begin(), keyOutputs.end(), index, [](const KeyOutputEntry& keyOutputEntry, uint32_t index)->bool { return keyOutputEntry.index < index; });
  if (iterator != keyOutputs.end() && iterator->index == index) {
    return KEY_OUTPUT;
  }

  return MULTISIGNATURE_OUTPUT;
}

const KeyOutput& BaseTransaction::getKeyOutput(uint32_t index) const {
  auto iterator = std::lower_bound(keyOutputs.begin(), keyOutputs.end(), index, [](const KeyOutputEntry& keyOutputEntry, uint32_t index)->bool { return keyOutputEntry.index < index; });
  assert(iterator != keyOutputs.end());
  assert(iterator->index == index);
  return iterator->output;
}

const MultisignatureOutput& BaseTransaction::getMultisignatureOutput(uint32_t index) const {
  auto iterator = std::lower_bound(multisignatureOutputs.begin(), multisignatureOutputs.end(), index, [](const MultisignatureOutputEntry& multisignatureOutputEntry, uint32_t index)->bool { return multisignatureOutputEntry.index < index; });
  assert(iterator != multisignatureOutputs.end());
  assert(iterator->index == index);
  return iterator->output;
}

const std::vector<uint8_t>& BaseTransaction::getExtra() const {
  return extra;
}

}
