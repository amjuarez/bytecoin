// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>

namespace CryptoNote {
class DepositIndex {
public:
  using DepositAmount = int64_t;
  using DepositInterest = uint64_t;
  using DepositHeight = uint32_t;
  DepositIndex();
  explicit DepositIndex(DepositHeight expectedHeight);
  void pushBlock(DepositAmount amount, DepositInterest interest); 
  void popBlock(); 
  void reserve(DepositHeight expectedHeight);
  size_t popBlocks(DepositHeight from); 
  DepositAmount depositAmountAtHeight(DepositHeight height) const;
  DepositAmount fullDepositAmount() const; 
  DepositInterest depositInterestAtHeight(DepositHeight height) const;
  DepositInterest fullInterestAmount() const; 
  DepositHeight lastHeight() const;
  template <class Archive> void serialize(Archive& ar, const unsigned int version) {
    ar & index;
  }

private:
  struct DepositIndexEntry {
    DepositHeight height;
    DepositAmount amount;
    DepositInterest interest;
    template <class Archive> void serialize(Archive& ar, const unsigned int version) {
      ar & height;
      ar & amount;
      ar & interest;
    }
  };

  using IndexType = std::vector<DepositIndexEntry>;
  IndexType::const_iterator elementAt(DepositHeight height) const;
  IndexType index;
  DepositHeight height;
};
}
