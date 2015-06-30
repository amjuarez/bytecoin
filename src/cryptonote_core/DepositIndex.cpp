// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cryptonote_core/DepositIndex.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>

namespace CryptoNote {

DepositIndex::DepositIndex() {
  index.push_back({0, 0, 0});
  height = 0;
}

DepositIndex::DepositIndex(DepositHeight expectedHeight) {
  index.reserve(expectedHeight + 1);
  index.push_back({0, 0, 0});
  height = 0;
}

void DepositIndex::reserve(DepositHeight expectedHeight) {
  index.reserve(expectedHeight + 1);
}

auto DepositIndex::fullDepositAmount() const -> DepositAmount {
  assert(!index.empty());
  return index.back().amount;
}

auto DepositIndex::fullInterestAmount() const -> DepositInterest {
  assert(!index.empty());
  return index.back().interest;
}

static inline bool sumWillOverflow(int64_t x, int64_t y) {
  if (y > 0 && x > std::numeric_limits<int64_t>::max() - y) {
    return true;
  }

  if (y < 0 && x < std::numeric_limits<int64_t>::min() - y) {
    return true;
  }
  
  return false;
}

static inline bool sumWillOverflow(uint64_t x, uint64_t y) {
  if (x > std::numeric_limits<uint64_t>::max() - y) {
    return true;
  }
 
  return false;
}

void DepositIndex::pushBlock(DepositAmount amount, DepositInterest interest) {
  auto lastAmount = index.back().amount;
  auto lastInterest = index.back().interest;
  assert(!sumWillOverflow(interest, lastInterest));
  assert(!sumWillOverflow(amount, lastAmount));
  assert(amount + lastAmount >= 0);
  ++height;
  if (amount != 0 || interest > 0) {
    index.push_back({height, amount + lastAmount, interest + lastInterest});
  }
}

void DepositIndex::popBlock() {
  assert(!index.empty());
  assert(height > 0);
  if (index.back().height == height) {
    assert(index.size() > 1);
    index.pop_back();
  }

  --height;
}
  
auto DepositIndex::lastHeight() const -> DepositHeight {
  return height;
}

auto DepositIndex::elementAt(DepositHeight height) const -> IndexType::const_iterator {
  return std::upper_bound(
      index.cbegin(), index.cend(), height,
      [] (DepositHeight height, const DepositIndexEntry& left) { return height < left.height; }) - 1;
}

size_t DepositIndex::popBlocks(DepositHeight from) {
  from = from == 0 ? 1 : from;
  if (from > height) {
    return 0;
  }

  IndexType::iterator it = index.begin();
  std::advance(it, std::distance(index.cbegin(), elementAt(from)));
  if (it->height < from) {
    ++it;
  }

  auto diff = height - from + 1;
  index.erase(it, index.end());
  height -= diff;
  return diff;
}

auto DepositIndex::depositAmountAtHeight(DepositHeight height) const -> DepositAmount {
  assert(!index.empty());
  return elementAt(height)->amount;
}

auto DepositIndex::depositInterestAtHeight(DepositHeight height) const -> DepositInterest {
  assert(!index.empty());
  return elementAt(height)->interest;
}
}
