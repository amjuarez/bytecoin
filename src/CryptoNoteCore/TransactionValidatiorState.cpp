// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
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

#include "TransactionValidatiorState.h"

namespace CryptoNote {

void mergeStates(TransactionValidatorState& destionation, const TransactionValidatorState& source) {
  destionation.spentKeyImages.insert(source.spentKeyImages.begin(), source.spentKeyImages.end());
  destionation.spentMultisignatureGlobalIndexes.insert(source.spentMultisignatureGlobalIndexes.begin(), source.spentMultisignatureGlobalIndexes.end());
}

bool hasIntersections(const TransactionValidatorState& destination, const TransactionValidatorState& source) {
  return std::any_of(source.spentKeyImages.begin(), source.spentKeyImages.end(),
                     [&](const Crypto::KeyImage& ki) { return destination.spentKeyImages.count(ki) != 0; }) ||
         std::any_of(source.spentMultisignatureGlobalIndexes.begin(), source.spentMultisignatureGlobalIndexes.end(),
                     [&](const std::pair<uint64_t, uint32_t>& pr) {
                       return destination.spentMultisignatureGlobalIndexes.count(pr) != 0;
                     });
}

void excludeFromState(TransactionValidatorState& state, const CachedTransaction& cachedTransaction) {
  const auto& transaction = cachedTransaction.getTransaction();
  for (auto& input : transaction.inputs) {
    if (input.type() == typeid(KeyInput)) {
      const auto& in = boost::get<KeyInput>(input);
      assert(state.spentKeyImages.count(in.keyImage) > 0);
      state.spentKeyImages.erase(in.keyImage);
    } else if (input.type() == typeid(MultisignatureInput)) {
      const auto& in = boost::get<MultisignatureInput>(input);
      assert(state.spentMultisignatureGlobalIndexes.count({in.amount, in.outputIndex}) > 0);
      state.spentMultisignatureGlobalIndexes.erase({in.amount, in.outputIndex});
    } else {
      assert(false);
    }
  }
}

}
