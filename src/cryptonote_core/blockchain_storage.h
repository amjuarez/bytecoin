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

#include <atomic>

#include "google/sparse_hash_set"
#include "google/sparse_hash_map"

#include "common/ObserverManager.h"
#include "common/util.h"
#include "cryptonote_core/BlockIndex.h"
#include "cryptonote_core/checkpoints.h"
#include "cryptonote_core/Currency.h"
#include "cryptonote_core/IBlockchainStorageObserver.h"
#include "cryptonote_core/ITransactionValidator.h"
#include "cryptonote_core/SwappedVector.h"
#include "cryptonote_core/UpgradeDetector.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/tx_pool.h"


namespace cryptonote {
  struct NOTIFY_RESPONSE_CHAIN_ENTRY_request;
  struct NOTIFY_REQUEST_GET_OBJECTS_request;
  struct NOTIFY_RESPONSE_GET_OBJECTS_request;
  struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request;
  struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response;
  struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount;

  using CryptoNote::BlockInfo;
  class blockchain_storage : public CryptoNote::ITransactionValidator {
  public:
    blockchain_storage(const Currency& currency, tx_memory_pool& tx_pool);

    bool addObserver(IBlockchainStorageObserver* observer);
    bool removeObserver(IBlockchainStorageObserver* observer);

    // ITransactionValidator
    virtual bool checkTransactionInputs(const cryptonote::Transaction& tx, BlockInfo& maxUsedBlock);
    virtual bool checkTransactionInputs(const cryptonote::Transaction& tx, BlockInfo& maxUsedBlock, BlockInfo& lastFailed);
    virtual bool haveSpentKeyImages(const cryptonote::Transaction& tx);

    bool init() { return init(tools::get_default_data_dir(), true); }
    bool init(const std::string& config_folder, bool load_existing);
    bool deinit();

    bool getLowerBound(uint64_t timestamp, uint64_t startOffset, uint64_t& height);
    bool getBlockIds(uint64_t startHeight, size_t maxCount, std::list<crypto::hash>& items);

    void set_checkpoints(checkpoints&& chk_pts) { m_checkpoints = chk_pts; }
    bool get_blocks(uint64_t start_offset, size_t count, std::list<Block>& blocks, std::list<Transaction>& txs);
    bool get_blocks(uint64_t start_offset, size_t count, std::list<Block>& blocks);
    bool get_alternative_blocks(std::list<Block>& blocks);
    size_t get_alternative_blocks_count();
    crypto::hash get_block_id_by_height(uint64_t height);
    bool get_block_by_hash(const crypto::hash &h, Block &blk);

    template<class archive_t> void serialize(archive_t & ar, const unsigned int version);

    bool have_tx(const crypto::hash &id);
    bool have_tx_keyimges_as_spent(const Transaction &tx);

    uint64_t get_current_blockchain_height();
    crypto::hash get_tail_id();
    crypto::hash get_tail_id(uint64_t& height);
    difficulty_type get_difficulty_for_next_block();
    uint64_t getCoinsInCirculation();
    uint8_t get_block_major_version_for_height(uint64_t height) const;
    bool add_new_block(const Block& bl_, block_verification_context& bvc);
    bool reset_and_set_genesis_block(const Block& b);
    bool create_block_template(Block& b, const AccountPublicAddress& miner_address, difficulty_type& di, uint64_t& height, const blobdata& ex_nonce);
    bool have_block(const crypto::hash& id);
    size_t get_total_transactions();
    bool get_short_chain_history(std::list<crypto::hash>& ids);
    bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, uint64_t& starter_offset); // !!!!
    bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, NOTIFY_RESPONSE_CHAIN_ENTRY_request& resp);
    bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<Block, std::list<Transaction>>>& blocks, uint64_t& total_height, uint64_t& start_height, size_t max_count);
    bool handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS_request& arg, NOTIFY_RESPONSE_GET_OBJECTS_request& rsp);
    bool get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& res);
    bool get_backward_blocks_sizes(size_t from_height, std::vector<size_t>& sz, size_t count);
    bool get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs);
    bool check_tx_inputs(const Transaction& tx, uint64_t& pmax_used_block_height, crypto::hash& max_used_block_id, BlockInfo* tail = 0);
    uint64_t get_current_comulative_blocksize_limit();
    bool is_storing_blockchain(){return m_is_blockchain_storing;}
    uint64_t block_difficulty(size_t i);
    bool getPoolSymmetricDifference(const std::vector<crypto::hash>& known_pool_tx_ids, const crypto::hash& known_block_id, std::vector<Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids);


    template<class t_ids_container, class t_blocks_container, class t_missed_container>
    bool get_blocks(const t_ids_container& block_ids, t_blocks_container& blocks, t_missed_container& missed_bs) {
      CRITICAL_REGION_LOCAL(m_blockchain_lock);

      for (const auto& bl_id : block_ids) {
        uint64_t height = 0;
        if (!m_blockIndex.getBlockHeight(bl_id, height)) {
          missed_bs.push_back(bl_id);
        } else {
          CHECK_AND_ASSERT_MES(height < m_blocks.size(), false, "Internal error: bl_id=" << epee::string_tools::pod_to_hex(bl_id)
            << " have index record with offset=" << height << ", bigger then m_blocks.size()=" << m_blocks.size());
            blocks.push_back(m_blocks[height].bl);
        }
      }

      return true;
    }

    template<class t_ids_container, class t_tx_container, class t_missed_container>
    void get_transactions(const t_ids_container& txs_ids, t_tx_container& txs, t_missed_container& missed_txs, bool checkTxPool = false) {
      CRITICAL_REGION_LOCAL(m_blockchain_lock);

      for (const auto& tx_id : txs_ids) {
        auto it = m_transactionMap.find(tx_id);
        if (it == m_transactionMap.end()) {
          missed_txs.push_back(tx_id);
        } else {
          txs.push_back(transactionByIndex(it->second).tx);
        }
      }

      if (checkTxPool) {
        auto poolTxIds = std::move(missed_txs);
        missed_txs.clear();
        m_tx_pool.getTransactions(poolTxIds, txs, missed_txs);
      }
    }

    //debug functions
    void print_blockchain(uint64_t start_index, uint64_t end_index);
    void print_blockchain_index();
    void print_blockchain_outs(const std::string& file);

  private:
    struct TransactionEntry {
      Transaction tx;
      std::vector<uint32_t> m_global_output_indexes;

      template<class archive_t> void serialize(archive_t & ar, unsigned int version);

      BEGIN_SERIALIZE_OBJECT()
        FIELD(tx)
        FIELD(m_global_output_indexes)
      END_SERIALIZE()
    };

    struct BlockEntry {
      Block bl;
      uint32_t height;
      uint64_t block_cumulative_size;
      difficulty_type cumulative_difficulty;
      uint64_t already_generated_coins;
      std::vector<TransactionEntry> transactions;

      template<class Archive> void serialize(Archive& archive, unsigned int version);

      BEGIN_SERIALIZE_OBJECT()
        FIELD(bl)
        VARINT_FIELD(height)
        VARINT_FIELD(block_cumulative_size)
        VARINT_FIELD(cumulative_difficulty)
        VARINT_FIELD(already_generated_coins)
        FIELD(transactions)
      END_SERIALIZE()
    };

    struct TransactionIndex {
      uint32_t block;
      uint16_t transaction;

      template<class Archive> void serialize(Archive& archive, unsigned int version);
    };

    struct MultisignatureOutputUsage {
      TransactionIndex transactionIndex;
      uint16_t outputIndex;
      bool isUsed;

      template<class Archive> void serialize(Archive& archive, unsigned int version);
    };

    typedef google::sparse_hash_set<crypto::key_image> key_images_container;
    typedef std::unordered_map<crypto::hash, BlockEntry> blocks_ext_by_hash;
    typedef google::sparse_hash_map<uint64_t, std::vector<std::pair<TransactionIndex, uint16_t>>> outputs_container; //crypto::hash - tx hash, size_t - index of out in transaction
    typedef std::map<uint64_t, std::vector<MultisignatureOutputUsage>> MultisignatureOutputsContainer;

    const Currency& m_currency;
    tx_memory_pool& m_tx_pool;
    epee::critical_section m_blockchain_lock; // TODO: add here reader/writer lock
    crypto::cn_context m_cn_context;
    tools::ObserverManager<IBlockchainStorageObserver> m_observerManager;

    key_images_container m_spent_keys;
    size_t m_current_block_cumul_sz_limit;
    blocks_ext_by_hash m_alternative_chains; // crypto::hash -> block_extended_info
    outputs_container m_outputs;

    std::string m_config_folder;
    checkpoints m_checkpoints;
    std::atomic<bool> m_is_in_checkpoint_zone;
    std::atomic<bool> m_is_blockchain_storing;

    typedef SwappedVector<BlockEntry> Blocks;
    typedef std::unordered_map<crypto::hash, uint32_t> BlockMap;
    typedef std::unordered_map<crypto::hash, TransactionIndex> TransactionMap;
    typedef BasicUpgradeDetector<Blocks> UpgradeDetector;

    friend class BlockCacheSerializer;

    Blocks m_blocks;
    CryptoNote::BlockIndex m_blockIndex;
    TransactionMap m_transactionMap;
    MultisignatureOutputsContainer m_multisignatureOutputs;
    UpgradeDetector m_upgradeDetector;

    bool storeCache();
    template<class visitor_t> bool scan_outputkeys_for_indexes(const TransactionInputToKey& tx_in_to_key, visitor_t& vis, uint64_t* pmax_related_block_height = NULL);
    bool switch_to_alternative_blockchain(std::list<blocks_ext_by_hash::iterator>& alt_chain, bool discard_disconnected_chain);
    bool handle_alternative_block(const Block& b, const crypto::hash& id, block_verification_context& bvc);
    difficulty_type get_next_difficulty_for_alternative_chain(const std::list<blocks_ext_by_hash::iterator>& alt_chain, BlockEntry& bei);
    bool prevalidate_miner_transaction(const Block& b, uint64_t height);
    bool validate_miner_transaction(const Block& b, uint64_t height, size_t cumulativeBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee, uint64_t& reward, int64_t& emissionChange);
    bool validate_transaction(const Block& b, uint64_t height, const Transaction& tx);
    bool rollback_blockchain_switching(std::list<Block>& original_chain, size_t rollback_height);
    bool get_last_n_blocks_sizes(std::vector<size_t>& sz, size_t count);
    bool add_out_to_get_random_outs(std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount& result_outs, uint64_t amount, size_t i);
    bool is_tx_spendtime_unlocked(uint64_t unlock_time);
    size_t find_end_of_allowed_index(const std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs);
    bool check_block_timestamp_main(const Block& b);
    bool check_block_timestamp(std::vector<uint64_t> timestamps, const Block& b);
    uint64_t get_adjusted_time();
    bool complete_timestamps_vector(uint64_t start_height, std::vector<uint64_t>& timestamps);
    bool checkBlockVersion(const Block& b, const crypto::hash& blockHash);
    bool checkParentBlockSize(const Block& b, const crypto::hash& blockHash);
    bool checkCumulativeBlockSize(const crypto::hash& blockId, size_t cumulativeBlockSize, uint64_t height);
    bool getBlockCumulativeSize(const Block& block, size_t& cumulativeSize);
    bool update_next_comulative_size_limit();
    bool check_tx_input(const TransactionInputToKey& txin, const crypto::hash& tx_prefix_hash, const std::vector<crypto::signature>& sig, uint64_t* pmax_related_block_height = NULL);
    bool check_tx_inputs(const Transaction& tx, const crypto::hash& tx_prefix_hash, uint64_t* pmax_used_block_height = NULL);
    bool check_tx_inputs(const Transaction& tx, uint64_t* pmax_used_block_height = NULL);
    bool have_tx_keyimg_as_spent(const crypto::key_image &key_im);
    const TransactionEntry& transactionByIndex(TransactionIndex index);
    bool pushBlock(const Block& blockData, block_verification_context& bvc);
    bool pushBlock(BlockEntry& block);
    void popBlock(const crypto::hash& blockHash);
    bool pushTransaction(BlockEntry& block, const crypto::hash& transactionHash, TransactionIndex transactionIndex);
    void popTransaction(const Transaction& transaction, const crypto::hash& transactionHash);
    void popTransactions(const BlockEntry& block, const crypto::hash& minerTransactionHash);
    bool validateInput(const TransactionInputMultisignature& input, const crypto::hash& transactionHash, const crypto::hash& transactionPrefixHash, const std::vector<crypto::signature>& transactionSignatures);

    friend class LockedBlockchainStorage;
  };

  class LockedBlockchainStorage: boost::noncopyable {
  public:

    LockedBlockchainStorage(blockchain_storage& bc)
      : m_bc(bc), m_lock(bc.m_blockchain_lock) {}

    blockchain_storage* operator -> () {
      return &m_bc;
    }

  private:

    blockchain_storage& m_bc;
    epee::critical_region_t<epee::critical_section> m_lock;
  };

  template<class visitor_t> bool blockchain_storage::scan_outputkeys_for_indexes(const TransactionInputToKey& tx_in_to_key, visitor_t& vis, uint64_t* pmax_related_block_height) {
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    auto it = m_outputs.find(tx_in_to_key.amount);
    if (it == m_outputs.end() || !tx_in_to_key.keyOffsets.size())
      return false;

    std::vector<uint64_t> absolute_offsets = relative_output_offsets_to_absolute(tx_in_to_key.keyOffsets);
    std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs_vec = it->second;
    size_t count = 0;
    for (uint64_t i : absolute_offsets) {
      if(i >= amount_outs_vec.size() ) {
        LOG_PRINT_L0("Wrong index in transaction inputs: " << i << ", expected maximum " << amount_outs_vec.size() - 1);
        return false;
      }

      //auto tx_it = m_transactionMap.find(amount_outs_vec[i].first);
      //CHECK_AND_ASSERT_MES(tx_it != m_transactionMap.end(), false, "Wrong transaction id in output indexes: " << epee::string_tools::pod_to_hex(amount_outs_vec[i].first));

      const TransactionEntry& tx = transactionByIndex(amount_outs_vec[i].first);
      CHECK_AND_ASSERT_MES(amount_outs_vec[i].second < tx.tx.vout.size(), false,
        "Wrong index in transaction outputs: " << amount_outs_vec[i].second << ", expected less then " << tx.tx.vout.size());
      if (!vis.handle_output(tx.tx, tx.tx.vout[amount_outs_vec[i].second])) {
        LOG_PRINT_L0("Failed to handle_output for output no = " << count << ", with absolute offset " << i);
        return false;
      }

      if(count++ == absolute_offsets.size()-1 && pmax_related_block_height) {
        if (*pmax_related_block_height < amount_outs_vec[i].first.block) {
          *pmax_related_block_height = amount_outs_vec[i].first.block;
        }
      }
    }

    return true;
  }
}

#include "cryptonote_boost_serialization.h"
#include "blockchain_storage_boost_serialization.h"
