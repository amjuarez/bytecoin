// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "BlockIndex.h"
#include <boost/utility/value_init.hpp>

namespace CryptoNote
{
  crypto::hash BlockIndex::getBlockId(uint64_t height) const {
    if (height >= m_container.size())
      return boost::value_initialized<crypto::hash>();
    return m_container[static_cast<size_t>(height)];
  }

  bool BlockIndex::getBlockIds(uint64_t startHeight, size_t maxCount, std::list<crypto::hash>& items) const {
    if (startHeight >= m_container.size())
      return false;

    for (size_t i = startHeight; i < (startHeight + maxCount) && i < m_container.size(); ++i) {
      items.push_back(m_container[i]);
    }

    return true;
  }


  bool BlockIndex::findSupplement(const std::list<crypto::hash>& ids, uint64_t& offset) const {

    for (const auto& id : ids) {
      if (getBlockHeight(id, offset))
        return true;
    }

    return false;
  }

  bool BlockIndex::getShortChainHistory(std::list<crypto::hash>& ids) const {
    size_t i = 0;
    size_t current_multiplier = 1;
    size_t sz = size();
    
    if (!sz)
      return true;

    size_t current_back_offset = 1;
    bool genesis_included = false;

    while (current_back_offset < sz) {
      ids.push_back(m_container[sz - current_back_offset]);
      if (sz - current_back_offset == 0)
        genesis_included = true;
      if (i < 10) {
        ++current_back_offset;
      } else {
        current_back_offset += current_multiplier *= 2;
      }
      ++i;
    }

    if (!genesis_included)
      ids.push_back(m_container[0]);

    return true;
  }

  crypto::hash BlockIndex::getTailId() const {
    if (m_container.empty())
      return boost::value_initialized<crypto::hash>();
    return m_container.back();
  }


}
