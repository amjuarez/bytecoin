// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// multi index
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>

#include "crypto/hash.h"
#include <list>

namespace CryptoNote
{
  class BlockIndex {

  public:

    BlockIndex() : 
      m_index(m_container.get<1>()) {}

    void pop() {
      m_container.pop_back();
    }

    // returns true if new element was inserted, false if already exists
    bool push(const crypto::hash& h) {
      auto result = m_container.push_back(h);
      return result.second;
    }

    bool hasBlock(const crypto::hash& h) const {
      return m_index.find(h) != m_index.end();
    }

    bool getBlockHeight(const crypto::hash& h, uint64_t& height) const {
      auto hi = m_index.find(h);
      if (hi == m_index.end())
        return false;

      height = std::distance(m_container.begin(), m_container.project<0>(hi));
      return true;
    }

    size_t size() const {
      return m_container.size();
    }

    void clear() {
      m_container.clear();
    }

    crypto::hash getBlockId(uint64_t height) const;
    bool getBlockIds(uint64_t startHeight, size_t maxCount, std::list<crypto::hash>& items) const;
    bool findSupplement(const std::list<crypto::hash>& ids, uint64_t& offset) const;
    bool getShortChainHistory(std::list<crypto::hash>& ids) const;
    crypto::hash getTailId() const;

    template <class Archive> void serialize(Archive& ar, const unsigned int version) {
      ar & m_container;
    }

  private:

    typedef boost::multi_index_container <
      crypto::hash,
      boost::multi_index::indexed_by<
        boost::multi_index::random_access<>,
        boost::multi_index::hashed_unique<boost::multi_index::identity<crypto::hash>>
      >
    > ContainerT;

    ContainerT m_container;
    ContainerT::nth_index<1>::type& m_index;

  };
}
