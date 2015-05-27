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

#include "Transaction.h"
#include <algorithm>
#include <cassert>

namespace CryptoNote {

Transaction::Transaction(
  uint64_t unlockTime,
  std::vector<KeyInputEntry>&& keyInputs,
  std::vector<MultisignatureInputEntry>&& multisignatureInputs,
  std::vector<KeyOutputEntry>&& keyOutputs,
  std::vector<MultisignatureOutputEntry>&& multisignatureOutputs,
  std::vector<uint8_t>&& extra) :
  unlockTime(unlockTime),
  keyInputs(std::move(keyInputs)),
  multisignatureInputs(std::move(multisignatureInputs)),
  keyOutputs(std::move(keyOutputs)),
  multisignatureOutputs(std::move(multisignatureOutputs)),
  extra(std::move(extra)) {
}

Transaction::Transaction(
  Transaction&& other) :
  unlockTime(other.unlockTime),
  keyInputs(std::move(other.keyInputs)),
  multisignatureInputs(std::move(other.multisignatureInputs)),
  keyOutputs(std::move(other.keyOutputs)),
  multisignatureOutputs(std::move(other.multisignatureOutputs)),
  extra(std::move(other.extra)) {
}

uint64_t Transaction::getUnlockTime() const {
  return unlockTime;
}

uint32_t Transaction::getInputCount() const {
  return static_cast<uint32_t>(keyInputs.size() + multisignatureInputs.size());
}

Transaction::InputType Transaction::getInputType(uint32_t index) const {
  auto iterator = std::lower_bound(keyInputs.begin(), keyInputs.end(), index, [](const KeyInputEntry& keyInputEntry, uint32_t index)->bool { return keyInputEntry.index < index; });
  if (iterator != keyInputs.end() && iterator->index == index) {
    return KEY_INPUT;
  }

  return MULTISIGNATURE_INPUT;
}

const KeyInput& Transaction::getKeyInput(uint32_t index) const {
  auto iterator = std::lower_bound(keyInputs.begin(), keyInputs.end(), index, [](const KeyInputEntry& keyInputEntry, uint32_t index)->bool { return keyInputEntry.index < index; });
  assert(iterator != keyInputs.end());
  assert(iterator->index == index);
  return iterator->input;
}

const MultisignatureInput& Transaction::getMultisignatureInput(uint32_t index) const {
  auto iterator = std::lower_bound(multisignatureInputs.begin(), multisignatureInputs.end(), index, [](const MultisignatureInputEntry& multisignatureInputEntry, uint32_t index)->bool { return multisignatureInputEntry.index < index; });
  assert(iterator != multisignatureInputs.end());
  assert(iterator->index == index);
  return iterator->input;
}

uint32_t Transaction::getOutputCount() const {
  return static_cast<uint32_t>(keyOutputs.size() + multisignatureOutputs.size());
}

Transaction::OutputType Transaction::getOutputType(uint32_t index) const {
  auto iterator = std::lower_bound(keyOutputs.begin(), keyOutputs.end(), index, [](const KeyOutputEntry& keyOutputEntry, uint32_t index)->bool { return keyOutputEntry.index < index; });
  if (iterator != keyOutputs.end() && iterator->index == index) {
    return KEY_OUTPUT;
  }

  return MULTISIGNATURE_OUTPUT;
}

const KeyOutput& Transaction::getKeyOutput(uint32_t index) const {
  auto iterator = std::lower_bound(keyOutputs.begin(), keyOutputs.end(), index, [](const KeyOutputEntry& keyOutputEntry, uint32_t index)->bool { return keyOutputEntry.index < index; });
  assert(iterator != keyOutputs.end());
  assert(iterator->index == index);
  return iterator->output;
}

const MultisignatureOutput& Transaction::getMultisignatureOutput(uint32_t index) const {
  auto iterator = std::lower_bound(multisignatureOutputs.begin(), multisignatureOutputs.end(), index, [](const MultisignatureOutputEntry& multisignatureOutputEntry, uint32_t index)->bool { return multisignatureOutputEntry.index < index; });
  assert(iterator != multisignatureOutputs.end());
  assert(iterator->index == index);
  return iterator->output;
}

const std::vector<uint8_t>& Transaction::getExtra() const {
  return extra;
}

}
