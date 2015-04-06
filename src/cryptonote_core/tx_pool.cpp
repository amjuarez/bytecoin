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

#include "tx_pool.h"

#include <algorithm>
#include <ctime>
#include <vector>
#include <unordered_set>

#include <boost/filesystem.hpp>

// epee
#include "misc_language.h"
#include "misc_log_ex.h"
#include "warnings.h"

#include "common/boost_serialization_helper.h"
#include "common/int-util.h"
#include "common/util.h"
#include "crypto/hash.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_boost_serialization.h"
#include "cryptonote_config.h"


DISABLE_VS_WARNINGS(4244 4345 4503) //'boost::foreach_detail_::or_' : decorated name length exceeded, name was truncated

namespace cryptonote {

  //---------------------------------------------------------------------------------
  // BlockTemplate
  //---------------------------------------------------------------------------------
  class BlockTemplate {
  public:

    bool addTransaction(const crypto::hash& txid, const Transaction& tx) {
      if (!canAdd(tx))
        return false;

      for (const auto& in : tx.vin) {
        if (in.type() == typeid(TransactionInputToKey)) {
          auto r = m_keyImages.insert(boost::get<TransactionInputToKey>(in).keyImage);
          (void)r; //just to make compiler to shut up
          assert(r.second);
        } else if (in.type() == typeid(TransactionInputMultisignature)) {
          const auto& msig = boost::get<TransactionInputMultisignature>(in);
          auto r = m_usedOutputs.insert(std::make_pair(msig.amount, msig.outputIndex));
          (void)r; //just to make compiler to shut up
          assert(r.second);
        }
      }

      m_txHashes.push_back(txid);
      return true;
    }

    const std::vector<crypto::hash>& getTransactions() const {
      return m_txHashes;
    }

  private:

    bool canAdd(const Transaction& tx) {
      for (const auto& in : tx.vin) {
        if (in.type() == typeid(TransactionInputToKey)) {
          if (m_keyImages.count(boost::get<TransactionInputToKey>(in).keyImage)) {
            return false;
          }
        } else if (in.type() == typeid(TransactionInputMultisignature)) {
          const auto& msig = boost::get<TransactionInputMultisignature>(in);
          if (m_usedOutputs.count(std::make_pair(msig.amount, msig.outputIndex))) {
            return false;
          }
        }
      }
      return true;
    }
    
    std::unordered_set<crypto::key_image> m_keyImages;
    std::set<std::pair<uint64_t, uint64_t>> m_usedOutputs;
    std::vector<crypto::hash> m_txHashes;
  };

  using CryptoNote::BlockInfo;

  //---------------------------------------------------------------------------------
  tx_memory_pool::tx_memory_pool(const cryptonote::Currency& currency, CryptoNote::ITransactionValidator& validator, CryptoNote::ITimeProvider& timeProvider) :
    m_currency(currency),
    m_validator(validator), 
    m_timeProvider(timeProvider), 
    m_txCheckInterval(60, timeProvider),
    m_fee_index(boost::get<1>(m_transactions)) {
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::add_tx(const Transaction &tx, /*const crypto::hash& tx_prefix_hash,*/ const crypto::hash &id, size_t blobSize, tx_verification_context& tvc, bool keptByBlock) {
    if (!check_inputs_types_supported(tx)) {
      tvc.m_verifivation_failed = true;
      return false;
    }

    uint64_t inputs_amount = 0;
    if (!get_inputs_money_amount(tx, inputs_amount)) {
      tvc.m_verifivation_failed = true;
      return false;
    }

    uint64_t outputs_amount = get_outs_money_amount(tx);

    if (outputs_amount >= inputs_amount) {
      LOG_PRINT_L0("transaction use more money then it has: use " << m_currency.formatAmount(outputs_amount) <<
        ", have " << m_currency.formatAmount(inputs_amount));
      tvc.m_verifivation_failed = true;
      return false;
    }

    const uint64_t fee = inputs_amount - outputs_amount;
    if (!keptByBlock && fee < m_currency.minimumFee()) {
      LOG_PRINT_L0("transaction fee is not enought: " << m_currency.formatAmount(fee) <<
        ", minumim fee: " << m_currency.formatAmount(m_currency.minimumFee()));
      tvc.m_verifivation_failed = true;
      tvc.m_tx_fee_too_small = true;
      return false;
    }

    //check key images for transaction if it is not kept by block
    if (!keptByBlock) {
      CRITICAL_REGION_LOCAL(m_transactions_lock);
      if (haveSpentInputs(tx)) {
        LOG_PRINT_L0("Transaction with id= " << id << " used already spent inputs");
        tvc.m_verifivation_failed = true;
        return false;
      }
    }

    BlockInfo maxUsedBlock;

    // check inputs
    bool inputsValid = m_validator.checkTransactionInputs(tx, maxUsedBlock);

    if (!inputsValid) {
      if (!keptByBlock) {
        LOG_PRINT_L0("tx used wrong inputs, rejected");
        tvc.m_verifivation_failed = true;
        return false;
      }

      maxUsedBlock.clear();
      tvc.m_verifivation_impossible = true;
    }

    CRITICAL_REGION_LOCAL(m_transactions_lock);

    // add to pool
    {
      TransactionDetails txd;

      txd.id = id;
      txd.blobSize = blobSize;
      txd.tx = tx;
      txd.fee = fee;
      txd.keptByBlock = keptByBlock;
      txd.receiveTime = m_timeProvider.now();

      txd.maxUsedBlock = maxUsedBlock;
      txd.lastFailedBlock.clear();

      auto txd_p = m_transactions.insert(std::move(txd));
      CHECK_AND_ASSERT_MES(txd_p.second, false, "transaction already exists at inserting in memory pool");
    }

    tvc.m_added_to_pool = true;

    if (inputsValid && fee > 0)
      tvc.m_should_be_relayed = true;

    tvc.m_verifivation_failed = true;

    if (!addTransactionInputs(id, tx, keptByBlock))
      return false;

    tvc.m_verifivation_failed = false;
    //succeed
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::add_tx(const Transaction &tx, tx_verification_context& tvc, bool keeped_by_block) {
    crypto::hash h = null_hash;
    size_t blobSize = 0;
    get_transaction_hash(tx, h, blobSize);
    return add_tx(tx, h, blobSize, tvc, keeped_by_block);
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::take_tx(const crypto::hash &id, Transaction &tx, size_t& blobSize, uint64_t& fee) {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    auto it = m_transactions.find(id);
    if (it == m_transactions.end()) {
      return false;
    }

    auto& txd = *it;

    tx = txd.tx;
    blobSize = txd.blobSize;
    fee = txd.fee;

    removeTransaction(it);
    return true;
  }
  //---------------------------------------------------------------------------------
  size_t tx_memory_pool::get_transactions_count() const {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    return m_transactions.size();
  }
  //---------------------------------------------------------------------------------
  void tx_memory_pool::get_transactions(std::list<Transaction>& txs) const {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    for (const auto& tx_vt : m_transactions) {
      txs.push_back(tx_vt.tx);
    }
  }
  //---------------------------------------------------------------------------------
  void tx_memory_pool::get_difference(const std::vector<crypto::hash>& known_tx_ids, std::vector<crypto::hash>& new_tx_ids, std::vector<crypto::hash>& deleted_tx_ids) const {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    std::unordered_set<crypto::hash> ready_tx_ids;
    for (const auto& tx : m_transactions) {
      TransactionCheckInfo checkInfo(tx);
      if (is_transaction_ready_to_go(tx.tx, checkInfo)) {
        ready_tx_ids.insert(tx.id);
      }
    }

    std::unordered_set<crypto::hash> known_set(known_tx_ids.begin(), known_tx_ids.end());
    for (auto it = ready_tx_ids.begin(), e = ready_tx_ids.end(); it != e;) {
      auto known_it = known_set.find(*it);
      if (known_it != known_set.end()) {
        known_set.erase(known_it);
        it = ready_tx_ids.erase(it);
      }
      else {
        ++it;
      }
    }

    new_tx_ids.assign(ready_tx_ids.begin(), ready_tx_ids.end());
    deleted_tx_ids.assign(known_set.begin(), known_set.end());
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::on_blockchain_inc(uint64_t new_block_height, const crypto::hash& top_block_id) {
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::on_blockchain_dec(uint64_t new_block_height, const crypto::hash& top_block_id) {
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::have_tx(const crypto::hash &id) const {
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    if (m_transactions.count(id)) {
      return true;
    }
    return false;
  }
  //---------------------------------------------------------------------------------
  void tx_memory_pool::lock() const {
    m_transactions_lock.lock();
  }
  //---------------------------------------------------------------------------------
  void tx_memory_pool::unlock() const {
    m_transactions_lock.unlock();
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::is_transaction_ready_to_go(const Transaction& tx, TransactionCheckInfo& txd) const {

    if (!m_validator.checkTransactionInputs(tx, txd.maxUsedBlock, txd.lastFailedBlock))
      return false;

    //if we here, transaction seems valid, but, anyway, check for key_images collisions with blockchain, just to be sure
    if (m_validator.haveSpentKeyImages(tx))
      return false;

    //transaction is ok.
    return true;
  }
  //---------------------------------------------------------------------------------
  std::string tx_memory_pool::print_pool(bool short_format) const {
    std::stringstream ss;
    CRITICAL_REGION_LOCAL(m_transactions_lock);
    for (const auto& txd : m_fee_index) {
      ss << "id: " << txd.id << std::endl;
      if (!short_format) {
        ss << obj_to_json_str(txd.tx) << std::endl;
      }
      ss << "blobSize: " << txd.blobSize << std::endl
        << "fee: " << m_currency.formatAmount(txd.fee) << std::endl
        << "keptByBlock: " << (txd.keptByBlock ? 'T' : 'F') << std::endl
        << "max_used_block_height: " << txd.maxUsedBlock.height << std::endl
        << "max_used_block_id: " << txd.maxUsedBlock.id << std::endl
        << "last_failed_height: " << txd.lastFailedBlock.height << std::endl
        << "last_failed_id: " << txd.lastFailedBlock.id << std::endl
        << "received: " << std::ctime(&txd.receiveTime) << std::endl;
    }

    return ss.str();
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::fill_block_template(Block& bl, size_t median_size, size_t maxCumulativeSize,
                                           uint64_t already_generated_coins, size_t& total_size, uint64_t& fee) {
    CRITICAL_REGION_LOCAL(m_transactions_lock);

    total_size = 0;
    fee = 0;

    size_t max_total_size = (125 * median_size) / 100 - m_currency.minerTxBlobReservedSize();
    max_total_size = std::min(max_total_size, maxCumulativeSize);

    BlockTemplate blockTemplate;

    for (auto i = m_fee_index.begin(); i != m_fee_index.end(); ++i) {
      const auto& txd = *i;

      if (max_total_size < total_size + txd.blobSize) {
        continue;
      }

      TransactionCheckInfo checkInfo(txd);
      bool ready = is_transaction_ready_to_go(txd.tx, checkInfo);

      // update item state
      m_fee_index.modify(i, [&checkInfo](TransactionCheckInfo& item) {
        item = checkInfo;
      });
      
      if (ready && blockTemplate.addTransaction(txd.id, txd.tx)) {
        total_size += txd.blobSize;
        fee += txd.fee;
      }
    }

    bl.txHashes = blockTemplate.getTransactions();
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::init(const std::string& config_folder) {
    CRITICAL_REGION_LOCAL(m_transactions_lock);

    m_config_folder = config_folder;
    std::string state_file_path = config_folder + "/" + m_currency.txPoolFileName();
    boost::system::error_code ec;
    if (!boost::filesystem::exists(state_file_path, ec)) {
      return true;
    }
    bool res = tools::unserialize_obj_from_file(*this, state_file_path);
    if (!res) {
      LOG_ERROR("Failed to load memory pool from file " << state_file_path);

      m_transactions.clear();
      m_spent_key_images.clear();
      m_spentOutputs.clear();
    }
    // Ignore deserialization error
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::deinit() {
    if (!tools::create_directories_if_necessary(m_config_folder)) {
      LOG_PRINT_L0("Failed to create data directory: " << m_config_folder);
      return false;
    }

    std::string state_file_path = m_config_folder + "/" + m_currency.txPoolFileName();
    bool res = tools::serialize_obj_to_file(*this, state_file_path);
    if (!res) {
      LOG_PRINT_L0("Failed to serialize memory pool to file " << state_file_path);
    }
    return true;
  }

  //---------------------------------------------------------------------------------
  void tx_memory_pool::on_idle() {
    m_txCheckInterval.call([this](){ return removeExpiredTransactions(); });
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::removeExpiredTransactions() {
    bool somethingRemoved = false;
    {
      CRITICAL_REGION_LOCAL(m_transactions_lock);

      auto now = m_timeProvider.now();

      for (auto it = m_transactions.begin(); it != m_transactions.end();) {
        uint64_t txAge = now - it->receiveTime;
        bool remove = txAge > (it->keptByBlock ? m_currency.mempoolTxFromAltBlockLiveTime() : m_currency.mempoolTxLiveTime());

        if (remove) {
          LOG_PRINT_L2("Tx " << it->id << " removed from tx pool due to outdated, age: " << txAge);
          it = removeTransaction(it);
          somethingRemoved = true;
        } else {
          ++it;
        }
      }
    }

    if (somethingRemoved) {
      m_observerManager.notify(&ITxPoolObserver::txDeletedFromPool);
    }

    return true;
  }

  tx_memory_pool::tx_container_t::iterator tx_memory_pool::removeTransaction(tx_memory_pool::tx_container_t::iterator i) {
    removeTransactionInputs(i->id, i->tx, i->keptByBlock);
    return m_transactions.erase(i);
  }

  bool tx_memory_pool::removeTransactionInputs(const crypto::hash& tx_id, const Transaction& tx, bool keptByBlock) {
    for (const auto& in : tx.vin) {
      if (in.type() == typeid(TransactionInputToKey)) {
        const auto& txin = boost::get<TransactionInputToKey>(in);
        auto it = m_spent_key_images.find(txin.keyImage);
        CHECK_AND_ASSERT_MES(it != m_spent_key_images.end(), false, "failed to find transaction input in key images. img=" << txin.keyImage << std::endl
          << "transaction id = " << tx_id);
        std::unordered_set<crypto::hash>& key_image_set = it->second;
        CHECK_AND_ASSERT_MES(!key_image_set.empty(), false, "empty key_image set, img=" << txin.keyImage << std::endl
          << "transaction id = " << tx_id);

        auto it_in_set = key_image_set.find(tx_id);
        CHECK_AND_ASSERT_MES(it_in_set != key_image_set.end(), false, "transaction id not found in key_image set, img=" << txin.keyImage << std::endl
          << "transaction id = " << tx_id);
        key_image_set.erase(it_in_set);
        if (key_image_set.empty()) {
          //it is now empty hash container for this key_image
          m_spent_key_images.erase(it);
        }
      } else if (in.type() == typeid(TransactionInputMultisignature)) {
        if (!keptByBlock) {
          const auto& msig = boost::get<TransactionInputMultisignature>(in);
          auto output = GlobalOutput(msig.amount, msig.outputIndex);
          assert(m_spentOutputs.count(output));
          m_spentOutputs.erase(output);
        }
      }
    }

    return true;
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::addTransactionInputs(const crypto::hash& id, const Transaction& tx, bool keptByBlock) {
    // should not fail
    for (const auto& in : tx.vin) {
      if (in.type() == typeid(TransactionInputToKey)) {
        const auto& txin = boost::get<TransactionInputToKey>(in);
        std::unordered_set<crypto::hash>& kei_image_set = m_spent_key_images[txin.keyImage];
        CHECK_AND_ASSERT_MES(keptByBlock || kei_image_set.size() == 0, false, "internal error: keptByBlock=" << keptByBlock
          << ",  kei_image_set.size()=" << kei_image_set.size() << ENDL << "txin.keyImage=" << txin.keyImage << ENDL
          << "tx_id=" << id);
        auto ins_res = kei_image_set.insert(id);
        CHECK_AND_ASSERT_MES(ins_res.second, false, "internal error: try to insert duplicate iterator in key_image set");
      } else if (in.type() == typeid(TransactionInputMultisignature)) {
        if (!keptByBlock) {
          const auto& msig = boost::get<TransactionInputMultisignature>(in);
          auto r = m_spentOutputs.insert(GlobalOutput(msig.amount, msig.outputIndex));
          (void)r;
          assert(r.second);
        }
      }
    }

    return true;
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::haveSpentInputs(const Transaction& tx) const {
    for (const auto& in : tx.vin) {
      if (in.type() == typeid(TransactionInputToKey)) {
        const auto& tokey_in = boost::get<TransactionInputToKey>(in);
        if (m_spent_key_images.count(tokey_in.keyImage)) {
          return true;
        }
      } else if (in.type() == typeid(TransactionInputMultisignature)) {
        const auto& msig = boost::get<TransactionInputMultisignature>(in);
        if (m_spentOutputs.count(GlobalOutput(msig.amount, msig.outputIndex))) {
          return true;
        }
      }
    }
    return false;
  }

  bool tx_memory_pool::addObserver(ITxPoolObserver* observer) {
    return m_observerManager.add(observer);
  }

  bool tx_memory_pool::removeObserver(ITxPoolObserver* observer) {
    return m_observerManager.remove(observer);
  }
}
