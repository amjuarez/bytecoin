// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
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

#pragma once
#include "include_base_utils.h"

#include <set>
#include <unordered_map>
#include <unordered_set>

#include <boost/serialization/version.hpp>
#include <boost/utility.hpp>

// multi index
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

// epee
#include "math_helper.h"
#include "string_tools.h"
#include "syncobj.h"

#include "common/util.h"
#include "common/int-util.h"
#include "common/ObserverManager.h"
#include "crypto/hash.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/Currency.h"
#include "cryptonote_core/ITimeProvider.h"
#include "cryptonote_core/ITransactionValidator.h"
#include "cryptonote_core/ITxPoolObserver.h"
#include "cryptonote_core/verification_context.h"


namespace cryptonote {


  class OnceInTimeInterval {
  public:
    OnceInTimeInterval(unsigned interval, CryptoNote::ITimeProvider& timeProvider)
      : m_interval(interval), m_timeProvider(timeProvider) {
      m_lastWorkedTime = 0;
    }

    template<class functor_t>
    bool call(functor_t functr) {
      time_t now = m_timeProvider.now();

      if (now - m_lastWorkedTime > m_interval) {
        bool res = functr();
        m_lastWorkedTime = m_timeProvider.now();
        return res;
      }

      return true;
    }

  private:
    time_t m_lastWorkedTime;
    unsigned m_interval;
    CryptoNote::ITimeProvider& m_timeProvider;
  };

  using CryptoNote::BlockInfo;
  using namespace boost::multi_index;

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class tx_memory_pool: boost::noncopyable {
  public:
    tx_memory_pool(const cryptonote::Currency& currency, CryptoNote::ITransactionValidator& validator,
      CryptoNote::ITimeProvider& timeProvider);

    bool addObserver(ITxPoolObserver* observer);
    bool removeObserver(ITxPoolObserver* observer);

    // load/store operations
    bool init(const std::string& config_folder);
    bool deinit();

    bool have_tx(const crypto::hash &id) const;
    bool add_tx(const Transaction &tx, const crypto::hash &id, size_t blobSize, tx_verification_context& tvc, bool keeped_by_block);
    bool add_tx(const Transaction &tx, tx_verification_context& tvc, bool keeped_by_block);
    //gets tx and remove it from pool
    bool take_tx(const crypto::hash &id, Transaction &tx, size_t& blobSize, uint64_t& fee);

    bool on_blockchain_inc(uint64_t new_block_height, const crypto::hash& top_block_id);
    bool on_blockchain_dec(uint64_t new_block_height, const crypto::hash& top_block_id);

    void lock() const;
    void unlock() const;

    bool fill_block_template(Block &bl, size_t median_size, size_t maxCumulativeSize, uint64_t already_generated_coins, size_t &total_size, uint64_t &fee);

    void get_transactions(std::list<Transaction>& txs) const;
    void get_difference(const std::vector<crypto::hash>& known_tx_ids, std::vector<crypto::hash>& new_tx_ids, std::vector<crypto::hash>& deleted_tx_ids) const;
    size_t get_transactions_count() const;
    std::string print_pool(bool short_format) const;
    void on_idle();

    template<class t_ids_container, class t_tx_container, class t_missed_container>
    void getTransactions(const t_ids_container& txsIds, t_tx_container& txs, t_missed_container& missedTxs) {
      CRITICAL_REGION_LOCAL(m_transactions_lock);

      for (const auto& id : txsIds) {
        auto it = m_transactions.find(id);
        if (it == m_transactions.end()) {
          missedTxs.push_back(id);
        } else {
          txs.push_back(it->tx);
        }
      }
    }

#define CURRENT_MEMPOOL_ARCHIVE_VER    10

    template<class archive_t>
    void serialize(archive_t & a, const unsigned int version) {
      if (version < CURRENT_MEMPOOL_ARCHIVE_VER) {
        return;
      }

      CRITICAL_REGION_LOCAL(m_transactions_lock);
      a & m_transactions;
      a & m_spent_key_images;
      a & m_spentOutputs;
    }

    struct TransactionCheckInfo {
      BlockInfo maxUsedBlock;
      BlockInfo lastFailedBlock;
    };

    struct TransactionDetails : public TransactionCheckInfo {
      crypto::hash id;
      Transaction tx;
      size_t blobSize;
      uint64_t fee;
      bool keptByBlock;
      time_t receiveTime;
    };

  private:

    struct TransactionPriorityComparator {
      // lhs > hrs
      bool operator()(const TransactionDetails& lhs, const TransactionDetails& rhs) const {
        // price(lhs) = lhs.fee / lhs.blobSize
        // price(lhs) > price(rhs) -->
        // lhs.fee / lhs.blobSize > rhs.fee / rhs.blobSize -->
        // lhs.fee * rhs.blobSize > rhs.fee * lhs.blobSize
        uint64_t lhs_hi, lhs_lo = mul128(lhs.fee, rhs.blobSize, &lhs_hi);
        uint64_t rhs_hi, rhs_lo = mul128(rhs.fee, lhs.blobSize, &rhs_hi);

        return
          // prefer more profitable transactions
          (lhs_hi >  rhs_hi) ||
          (lhs_hi == rhs_hi && lhs_lo >  rhs_lo) ||
          // prefer smaller
          (lhs_hi == rhs_hi && lhs_lo == rhs_lo && lhs.blobSize <  rhs.blobSize) ||
          // prefer older
          (lhs_hi == rhs_hi && lhs_lo == rhs_lo && lhs.blobSize == rhs.blobSize && lhs.receiveTime < rhs.receiveTime);
      }
    };

    typedef hashed_unique<BOOST_MULTI_INDEX_MEMBER(TransactionDetails, crypto::hash, id)> main_index_t;
    typedef ordered_non_unique<identity<TransactionDetails>, TransactionPriorityComparator> fee_index_t;

    typedef multi_index_container<TransactionDetails,
      indexed_by<main_index_t, fee_index_t>
    > tx_container_t;

    typedef std::pair<uint64_t, uint64_t> GlobalOutput;
    typedef std::set<GlobalOutput> GlobalOutputsContainer;
    typedef std::unordered_map<crypto::key_image, std::unordered_set<crypto::hash> > key_images_container;


    // double spending checking
    bool addTransactionInputs(const crypto::hash& id, const Transaction& tx, bool keptByBlock);
    bool haveSpentInputs(const Transaction& tx) const;
    bool removeTransactionInputs(const crypto::hash& id, const Transaction& tx, bool keptByBlock);

    tx_container_t::iterator removeTransaction(tx_container_t::iterator i);
    bool removeExpiredTransactions();
    bool is_transaction_ready_to_go(const Transaction& tx, TransactionCheckInfo& txd) const;

    tools::ObserverManager<ITxPoolObserver> m_observerManager;

    const cryptonote::Currency& m_currency;
    OnceInTimeInterval m_txCheckInterval;
    mutable epee::critical_section m_transactions_lock;
    key_images_container m_spent_key_images;
    GlobalOutputsContainer m_spentOutputs;

    std::string m_config_folder;
    CryptoNote::ITransactionValidator& m_validator;
    CryptoNote::ITimeProvider& m_timeProvider;

    tx_container_t m_transactions;  
    tx_container_t::nth_index<1>::type& m_fee_index;

#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
    friend class blockchain_storage;
#endif
  };
}

namespace boost {
  namespace serialization {
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::tx_memory_pool::TransactionDetails& td, const unsigned int version) {
      ar & td.id;
      ar & td.blobSize;
      ar & td.fee;
      ar & td.tx;
      ar & td.maxUsedBlock.height;
      ar & td.maxUsedBlock.id;
      ar & td.lastFailedBlock.height;
      ar & td.lastFailedBlock.id;
      ar & td.keptByBlock;
      ar & td.receiveTime;
    }
  }
}

BOOST_CLASS_VERSION(cryptonote::tx_memory_pool, CURRENT_MEMPOOL_ARCHIVE_VER)
