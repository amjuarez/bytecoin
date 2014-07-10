// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <include_base_utils.h>
#include "blockchain_storage.h"

#include <algorithm>
#include <cstdio>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include "cryptonote_format_utils.h"
#include "cryptonote_boost_serialization.h"

#include "profile_tools.h"
#include "file_io_utils.h"
#include "common/boost_serialization_helper.h"

namespace {
  std::string appendPath(const std::string& path, const std::string& fileName) {
    std::string result = path;
    if (!result.empty()) {
      result += '/';
    }

    result += fileName;
    return result;
  }
}

namespace std {
  bool operator<(const crypto::hash& hash1, const crypto::hash& hash2) {
    return memcmp(&hash1, &hash2, crypto::HASH_SIZE) < 0;
  }

  bool operator<(const crypto::key_image& keyImage1, const crypto::key_image& keyImage2) {
    return memcmp(&keyImage1, &keyImage2, 32) < 0;
  }
}

using namespace cryptonote;

DISABLE_VS_WARNINGS(4267)

namespace cryptonote {
  struct transaction_chain_entry {
    transaction tx;
    uint64_t m_keeper_block_height;
    size_t m_blob_size;
    std::vector<uint64_t> m_global_output_indexes;
  };

  struct block_extended_info {
    block   bl;
    uint64_t height;
    size_t block_cumulative_size;
    difficulty_type cumulative_difficulty;
    uint64_t already_generated_coins;
  };
}

template<class Archive> void cryptonote::blockchain_storage::TransactionIndex::serialize(Archive& archive, unsigned int version) {
  archive & block;
  archive & transaction;
}

bool blockchain_storage::have_tx(const crypto::hash &id) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_transactionMap.find(id) != m_transactionMap.end();
}

bool blockchain_storage::have_tx_keyimg_as_spent(const crypto::key_image &key_im) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return  m_spent_keys.find(key_im) != m_spent_keys.end();
}

uint64_t blockchain_storage::get_current_blockchain_height() {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_blocks.size();
}

bool blockchain_storage::init(const std::string& config_folder, bool load_existing, bool testnet) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!config_folder.empty() && !tools::create_directories_if_necessary(config_folder)) {
    LOG_ERROR("Failed to create data directory: " << m_config_folder);
    return false;
  }

  m_config_folder = config_folder;

  if (!m_blocks.open(appendPath(config_folder, CRYPTONOTE_BLOCKS_FILENAME), appendPath(config_folder, CRYPTONOTE_BLOCKINDEXES_FILENAME), 1024)) {
    return false;
  }

  if (load_existing) {
    LOG_PRINT_L0("Loading blockchain...");

    if (m_blocks.empty()) {
      LOG_PRINT_L0("Can't load blockchain storage from file.");
    } else {
      bool rebuild = true;
      try {
        std::ifstream file(appendPath(config_folder, CRYPTONOTE_BLOCKSCACHE_FILENAME), std::ios::binary);
        boost::archive::binary_iarchive archive(file);
        crypto::hash lastBlockHash;
        archive & lastBlockHash;
        if (lastBlockHash == get_block_hash(m_blocks.back().bl)) {
          archive & m_blockMap;
          archive & m_transactionMap;
          archive & m_spent_keys;
          archive & m_outputs;
          rebuild = false;
        }
      } catch (std::exception&) {
      }

      if (rebuild) {
        LOG_PRINT_L0("No actual blockchain cache found, rebuilding internal structures...");
        std::chrono::steady_clock::time_point timePoint = std::chrono::steady_clock::now();
        for (uint32_t b = 0; b < m_blocks.size(); ++b) {
          const Block& block = m_blocks[b];
          crypto::hash blockHash = get_block_hash(block.bl);
          m_blockMap.insert(std::make_pair(blockHash, b));
          for (uint16_t t = 0; t < block.transactions.size(); ++t) {
            const Transaction& transaction = block.transactions[t];
            crypto::hash transactionHash = get_transaction_hash(transaction.tx);
            TransactionIndex transactionIndex = { b, t };
            m_transactionMap.insert(std::make_pair(transactionHash, transactionIndex));
            for (auto& i : transaction.tx.vin) {
              if (i.type() == typeid(txin_to_key)) {
                m_spent_keys.insert(::boost::get<txin_to_key>(i).k_image);
              }
            }

            for (uint16_t o = 0; o < transaction.tx.vout.size(); ++o) {
              m_outputs[transaction.tx.vout[o].amount].push_back(std::make_pair<>(transactionIndex, o));
            }
          }
        }

        std::chrono::duration<double> duration = std::chrono::steady_clock::now() - timePoint;
        LOG_PRINT_L0("Rebuilding internal structures took: " << duration.count());
      }
    }
  } else {
    m_blocks.clear();
  }

  if (m_blocks.empty()) {
    LOG_PRINT_L0("Blockchain not loaded, generating genesis block.");

    if (!storeGenesisBlock(testnet)) {
      return false;
    }
  } else {
    cryptonote::block b;
    if (testnet) {
      generateTestnetGenesisBlock(b);
    } else {
      generateGenesisBlock(b);
    }

    crypto::hash genesis_hash = get_block_hash(m_blocks[0].bl);
    crypto::hash testnet_genesis_hash = get_block_hash(b);
    if (genesis_hash != testnet_genesis_hash) {
      LOG_ERROR("Failed to init: genesis block mismatch. Probably you set --testnet flag with data dir with non-test blockchain or another network.");
      return false;
    }
  }

  uint64_t timestamp_diff = time(NULL) - m_blocks.back().bl.timestamp;
  if (!m_blocks.back().bl.timestamp)
    timestamp_diff = time(NULL) - 1341378000;
  LOG_PRINT_GREEN("Blockchain initialized. last block: " << m_blocks.size() - 1 << ", " << epee::misc_utils::get_time_interval_string(timestamp_diff) << " time ago, current difficulty: " << get_difficulty_for_next_block(), LOG_LEVEL_0);
  return true;
}

bool blockchain_storage::storeGenesisBlock(bool testnet) {
  block bl = ::boost::value_initialized<block>();
  block_verification_context bvc = boost::value_initialized<block_verification_context>();

  if (testnet) {
    generateTestnetGenesisBlock(bl);
  } else {
    generateGenesisBlock(bl);
  }

  add_new_block(bl, bvc);
  CHECK_AND_ASSERT_MES(!bvc.m_verifivation_failed, false, "Failed to add genesis block to blockchain");
  return true;
}

bool blockchain_storage::store_blockchain() {
  try {
    std::ofstream file(appendPath(m_config_folder, CRYPTONOTE_BLOCKSCACHE_FILENAME), std::ios::binary);
    boost::archive::binary_oarchive archive(file);
    crypto::hash lastBlockHash = get_block_hash(m_blocks.back().bl);
    archive & lastBlockHash;
    archive & m_blockMap;
    archive & m_transactionMap;
    archive & m_spent_keys;
    archive & m_outputs;
    LOG_PRINT_L0("Saved blockchain cache.");
  } catch (std::exception& e) {
    LOG_ERROR("Failed to save blockchain cache, " << e.what());
  }

  return true;
}

bool blockchain_storage::deinit() {
  return store_blockchain();
}

bool blockchain_storage::reset_and_set_genesis_block(const block& b) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  m_blocks.clear();
  m_blockMap.clear();
  m_transactionMap.clear();

  m_spent_keys.clear();
  m_alternative_chains.clear();
  m_outputs.clear();

  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  add_new_block(b, bvc);
  return bvc.m_added_to_main_chain && !bvc.m_verifivation_failed;
}

crypto::hash blockchain_storage::get_tail_id(uint64_t& height) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  height = get_current_blockchain_height() - 1;
  return get_tail_id();
}

crypto::hash blockchain_storage::get_tail_id() {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  crypto::hash id = null_hash;
  if (m_blocks.size()) {
    get_block_hash(m_blocks.back().bl, id);
  }

  return id;
}

bool blockchain_storage::get_short_chain_history(std::list<crypto::hash>& ids) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  size_t i = 0;
  size_t current_multiplier = 1;
  size_t sz = m_blocks.size();
  if (!sz)
    return true;
  size_t current_back_offset = 1;
  bool genesis_included = false;
  while (current_back_offset < sz)
  {
    ids.push_back(get_block_hash(m_blocks[sz - current_back_offset].bl));
    if (sz - current_back_offset == 0)
      genesis_included = true;
    if (i < 10)
    {
      ++current_back_offset;
    } else
    {
      current_back_offset += current_multiplier *= 2;
    }
    ++i;
  }
  if (!genesis_included)
    ids.push_back(get_block_hash(m_blocks[0].bl));

  return true;
}

crypto::hash blockchain_storage::get_block_id_by_height(uint64_t height) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (height >= m_blocks.size())
    return null_hash;

  return get_block_hash(m_blocks[height].bl);
}

bool blockchain_storage::get_block_by_hash(const crypto::hash& blockHash, block& b) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  auto blockIndexByHashIterator = m_blockMap.find(blockHash);
  if (blockIndexByHashIterator != m_blockMap.end()) {
    b = m_blocks[blockIndexByHashIterator->second].bl;
    return true;
  }

  auto blockByHashIterator = m_alternative_chains.find(blockHash);
  if (blockByHashIterator != m_alternative_chains.end()) {
    b = blockByHashIterator->second.bl;
    return true;
  }

  return false;
}

difficulty_type blockchain_storage::get_difficulty_for_next_block() {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> commulative_difficulties;
  size_t offset = m_blocks.size() - std::min(m_blocks.size(), static_cast<uint64_t>(DIFFICULTY_BLOCKS_COUNT));
  if (offset == 0) {
    ++offset;
  }

  for (; offset < m_blocks.size(); offset++) {
    timestamps.push_back(m_blocks[offset].bl.timestamp);
    commulative_difficulties.push_back(m_blocks[offset].cumulative_difficulty);
  }

  return next_difficulty(timestamps, commulative_difficulties);
}

bool blockchain_storage::rollback_blockchain_switching(std::list<block>& original_chain, size_t rollback_height) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  //remove failed subchain
  for (size_t i = m_blocks.size() - 1; i >= rollback_height; i--)
  {
    popBlock(get_block_hash(m_blocks.back().bl));
    //bool r = pop_block_from_blockchain();
    //CHECK_AND_ASSERT_MES(r, false, "PANIC!!! failed to remove block while chain switching during the rollback!");
  }
  //return back original chain
  BOOST_FOREACH(auto& bl, original_chain)
  {
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    bool r = pushBlock(bl, bvc);
    CHECK_AND_ASSERT_MES(r && bvc.m_added_to_main_chain, false, "PANIC!!! failed to add (again) block while chain switching during the rollback!");
  }

  LOG_PRINT_L0("Rollback success.");
  return true;
}

bool blockchain_storage::switch_to_alternative_blockchain(std::list<blocks_ext_by_hash::iterator>& alt_chain, bool discard_disconnected_chain) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  CHECK_AND_ASSERT_MES(alt_chain.size(), false, "switch_to_alternative_blockchain: empty chain passed");

  size_t split_height = alt_chain.front()->second.height;
  CHECK_AND_ASSERT_MES(m_blocks.size() > split_height, false, "switch_to_alternative_blockchain: blockchain size is lower than split height");

  //disconnecting old chain
  std::list<block> disconnected_chain;
  for (size_t i = m_blocks.size() - 1; i >= split_height; i--) {
    block b = m_blocks[i].bl;
    popBlock(get_block_hash(b));
    //CHECK_AND_ASSERT_MES(r, false, "failed to remove block on chain switching");
    disconnected_chain.push_front(b);
  }

  //connecting new alternative chain
  for (auto alt_ch_iter = alt_chain.begin(); alt_ch_iter != alt_chain.end(); alt_ch_iter++) {
    auto ch_ent = *alt_ch_iter;
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    bool r = pushBlock(ch_ent->second.bl, bvc);
    if (!r || !bvc.m_added_to_main_chain) {
      LOG_PRINT_L0("Failed to switch to alternative blockchain");
      rollback_blockchain_switching(disconnected_chain, split_height);
      //add_block_as_invalid(ch_ent->second, get_block_hash(ch_ent->second.bl));
      LOG_PRINT_L0("The block was inserted as invalid while connecting new alternative chain,  block_id: " << get_block_hash(ch_ent->second.bl));
      m_alternative_chains.erase(ch_ent);

      for (auto alt_ch_to_orph_iter = ++alt_ch_iter; alt_ch_to_orph_iter != alt_chain.end(); alt_ch_to_orph_iter++) {
        //block_verification_context bvc = boost::value_initialized<block_verification_context>();
        //add_block_as_invalid((*alt_ch_iter)->second, (*alt_ch_iter)->first);
        m_alternative_chains.erase(*alt_ch_to_orph_iter);
      }

      return false;
    }
  }

  if (!discard_disconnected_chain) {
    //pushing old chain as alternative chain
    for (auto& old_ch_ent : disconnected_chain) {
      block_verification_context bvc = boost::value_initialized<block_verification_context>();
      bool r = handle_alternative_block(old_ch_ent, get_block_hash(old_ch_ent), bvc);
      if (!r) {
        LOG_ERROR("Failed to push ex-main chain blocks to alternative chain ");
        rollback_blockchain_switching(disconnected_chain, split_height);
        return false;
      }
    }
  }

  //removing all_chain entries from alternative chain
  for (auto ch_ent : alt_chain) {
    m_alternative_chains.erase(ch_ent);
  }

  LOG_PRINT_GREEN("REORGANIZE SUCCESS! on height: " << split_height << ", new blockchain size: " << m_blocks.size(), LOG_LEVEL_0);
  return true;
}

difficulty_type blockchain_storage::get_next_difficulty_for_alternative_chain(const std::list<blocks_ext_by_hash::iterator>& alt_chain, Block& bei) {
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> commulative_difficulties;
  if (alt_chain.size() < DIFFICULTY_BLOCKS_COUNT) {
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    size_t main_chain_stop_offset = alt_chain.size() ? alt_chain.front()->second.height : bei.height;
    size_t main_chain_count = DIFFICULTY_BLOCKS_COUNT - std::min(static_cast<size_t>(DIFFICULTY_BLOCKS_COUNT), alt_chain.size());
    main_chain_count = std::min(main_chain_count, main_chain_stop_offset);
    size_t main_chain_start_offset = main_chain_stop_offset - main_chain_count;

    if (!main_chain_start_offset)
      ++main_chain_start_offset; //skip genesis block
    for (; main_chain_start_offset < main_chain_stop_offset; ++main_chain_start_offset) {
      timestamps.push_back(m_blocks[main_chain_start_offset].bl.timestamp);
      commulative_difficulties.push_back(m_blocks[main_chain_start_offset].cumulative_difficulty);
    }

    CHECK_AND_ASSERT_MES((alt_chain.size() + timestamps.size()) <= DIFFICULTY_BLOCKS_COUNT, false, "Internal error, alt_chain.size()[" << alt_chain.size()
      << "] + vtimestampsec.size()[" << timestamps.size() << "] NOT <= DIFFICULTY_WINDOW[]" << DIFFICULTY_BLOCKS_COUNT);
    for (auto it : alt_chain) {
      timestamps.push_back(it->second.bl.timestamp);
      commulative_difficulties.push_back(it->second.cumulative_difficulty);
    }
  } else {
    timestamps.resize(std::min(alt_chain.size(), static_cast<size_t>(DIFFICULTY_BLOCKS_COUNT)));
    commulative_difficulties.resize(std::min(alt_chain.size(), static_cast<size_t>(DIFFICULTY_BLOCKS_COUNT)));
    size_t count = 0;
    size_t max_i = timestamps.size() - 1;
    BOOST_REVERSE_FOREACH(auto it, alt_chain) {
      timestamps[max_i - count] = it->second.bl.timestamp;
      commulative_difficulties[max_i - count] = it->second.cumulative_difficulty;
      count++;
      if (count >= DIFFICULTY_BLOCKS_COUNT) {
        break;
      }
    }
  }

  return next_difficulty(timestamps, commulative_difficulties);
}

bool blockchain_storage::prevalidate_miner_transaction(const block& b, uint64_t height) {
  CHECK_AND_ASSERT_MES(b.miner_tx.vin.size() == 1, false, "coinbase transaction in the block has no inputs");
  CHECK_AND_ASSERT_MES(b.miner_tx.vin[0].type() == typeid(txin_gen), false, "coinbase transaction in the block has the wrong type");
  if (boost::get<txin_gen>(b.miner_tx.vin[0]).height != height) {
    LOG_PRINT_RED_L0("The miner transaction in block has invalid height: " << boost::get<txin_gen>(b.miner_tx.vin[0]).height << ", expected: " << height);
    return false;
  }

  CHECK_AND_ASSERT_MES(b.miner_tx.unlock_time == height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW,
    false,
    "coinbase transaction transaction have wrong unlock time=" << b.miner_tx.unlock_time << ", expected " << height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);

  if (!check_outs_overflow(b.miner_tx)) {
    LOG_PRINT_RED_L0("miner transaction have money overflow in block " << get_block_hash(b));
    return false;
  }

  return true;
}

bool blockchain_storage::validate_miner_transaction(const block& b, size_t cumulative_block_size, uint64_t fee, uint64_t& base_reward, uint64_t already_generated_coins) {
  uint64_t money_in_use = 0;
  for (auto& o : b.miner_tx.vout) {
    money_in_use += o.amount;
  }

  std::vector<size_t> last_blocks_sizes;
  get_last_n_blocks_sizes(last_blocks_sizes, CRYPTONOTE_REWARD_BLOCKS_WINDOW);
  if (!get_block_reward(epee::misc_utils::median(last_blocks_sizes), cumulative_block_size, already_generated_coins, base_reward)) {
    LOG_PRINT_L0("block size " << cumulative_block_size << " is bigger than allowed for this blockchain");
    return false;
  }
  
  if (base_reward + fee < money_in_use) {
    LOG_ERROR("coinbase transaction spend too much money (" << print_money(money_in_use) << "). Block reward is " << print_money(base_reward + fee) << "(" << print_money(base_reward) << "+" << print_money(fee) << ")");
    return false;
  }

  if (base_reward + fee != money_in_use) {
    LOG_ERROR("coinbase transaction doesn't use full amount of block reward:  spent: "
      << print_money(money_in_use) << ",  block reward " << print_money(base_reward + fee) << "(" << print_money(base_reward) << "+" << print_money(fee) << ")");
    return false;
  }

  return true;
}

bool blockchain_storage::get_backward_blocks_sizes(size_t from_height, std::vector<size_t>& sz, size_t count) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  CHECK_AND_ASSERT_MES(from_height < m_blocks.size(), false, "Internal error: get_backward_blocks_sizes called with from_height=" << from_height << ", blockchain height = " << m_blocks.size());
  size_t start_offset = (from_height + 1) - std::min((from_height + 1), count);
  for (size_t i = start_offset; i != from_height + 1; i++) {
    sz.push_back(m_blocks[i].block_cumulative_size);
  }

  return true;
}

bool blockchain_storage::get_last_n_blocks_sizes(std::vector<size_t>& sz, size_t count) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!m_blocks.size()) {
    return true;
  }

  return get_backward_blocks_sizes(m_blocks.size() - 1, sz, count);
}

uint64_t blockchain_storage::get_current_comulative_blocksize_limit() {
  return m_current_block_cumul_sz_limit;
}

bool blockchain_storage::create_block_template(block& b, const account_public_address& miner_address, difficulty_type& diffic, uint64_t& height, const blobdata& ex_nonce) {
  size_t median_size;
  uint64_t already_generated_coins;

  CRITICAL_REGION_BEGIN(m_blockchain_lock);
  b.major_version = CURRENT_BLOCK_MAJOR_VERSION;
  b.minor_version = CURRENT_BLOCK_MINOR_VERSION;
  b.prev_id = get_tail_id();
  b.timestamp = time(NULL);
  height = m_blocks.size();
  diffic = get_difficulty_for_next_block();
  CHECK_AND_ASSERT_MES(diffic, false, "difficulty owverhead.");

  median_size = m_current_block_cumul_sz_limit / 2;
  already_generated_coins = m_blocks.back().already_generated_coins;

  CRITICAL_REGION_END();

  size_t txs_size;
  uint64_t fee;
  if (!m_tx_pool.fill_block_template(b, median_size, already_generated_coins, txs_size, fee)) {
    return false;
  }

#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
  size_t real_txs_size = 0;
  uint64_t real_fee = 0;
  CRITICAL_REGION_BEGIN(m_tx_pool.m_transactions_lock);
  BOOST_FOREACH(crypto::hash &cur_hash, b.tx_hashes) {
    auto cur_res = m_tx_pool.m_transactions.find(cur_hash);
    if (cur_res == m_tx_pool.m_transactions.end()) {
      LOG_ERROR("Creating block template: error: transaction not found");
      continue;
    }
    tx_memory_pool::tx_details &cur_tx = cur_res->second;
    real_txs_size += cur_tx.blob_size;
    real_fee += cur_tx.fee;
    if (cur_tx.blob_size != get_object_blobsize(cur_tx.tx)) {
      LOG_ERROR("Creating block template: error: invalid transaction size");
    }
    uint64_t inputs_amount;
    if (!get_inputs_money_amount(cur_tx.tx, inputs_amount)) {
      LOG_ERROR("Creating block template: error: cannot get inputs amount");
    } else if (cur_tx.fee != inputs_amount - get_outs_money_amount(cur_tx.tx)) {
      LOG_ERROR("Creating block template: error: invalid fee");
    }
  }
  if (txs_size != real_txs_size) {
    LOG_ERROR("Creating block template: error: wrongly calculated transaction size");
  }
  if (fee != real_fee) {
    LOG_ERROR("Creating block template: error: wrongly calculated fee");
  }
  CRITICAL_REGION_END();
  LOG_PRINT_L1("Creating block template: height " << height <<
    ", median size " << median_size <<
    ", already generated coins " << already_generated_coins <<
    ", transaction size " << txs_size <<
    ", fee " << fee);
#endif

  /*
     two-phase miner transaction generation: we don't know exact block size until we prepare block, but we don't know reward until we know
     block size, so first miner transaction generated with fake amount of money, and with phase we know think we know expected block size
     */
  //make blocks coin-base tx looks close to real coinbase tx to get truthful blob size
  bool r = construct_miner_tx(height, median_size, already_generated_coins, txs_size, fee, miner_address, b.miner_tx, ex_nonce, 11);
  CHECK_AND_ASSERT_MES(r, false, "Failed to construc miner tx, first chance");
  size_t cumulative_size = txs_size + get_object_blobsize(b.miner_tx);
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
  LOG_PRINT_L1("Creating block template: miner tx size " << get_object_blobsize(b.miner_tx) <<
    ", cumulative size " << cumulative_size);
#endif
  for (size_t try_count = 0; try_count != 10; ++try_count) {
    r = construct_miner_tx(height, median_size, already_generated_coins, cumulative_size, fee, miner_address, b.miner_tx, ex_nonce, 11);

    CHECK_AND_ASSERT_MES(r, false, "Failed to construc miner tx, second chance");
    size_t coinbase_blob_size = get_object_blobsize(b.miner_tx);
    if (coinbase_blob_size > cumulative_size - txs_size) {
      cumulative_size = txs_size + coinbase_blob_size;
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
      LOG_PRINT_L1("Creating block template: miner tx size " << coinbase_blob_size <<
        ", cumulative size " << cumulative_size << " is greater then before");
#endif
      continue;
    }

    if (coinbase_blob_size < cumulative_size - txs_size) {
      size_t delta = cumulative_size - txs_size - coinbase_blob_size;
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
      LOG_PRINT_L1("Creating block template: miner tx size " << coinbase_blob_size <<
        ", cumulative size " << txs_size + coinbase_blob_size <<
        " is less then before, adding " << delta << " zero bytes");
#endif
      b.miner_tx.extra.insert(b.miner_tx.extra.end(), delta, 0);
      //here  could be 1 byte difference, because of extra field counter is varint, and it can become from 1-byte len to 2-bytes len.
      if (cumulative_size != txs_size + get_object_blobsize(b.miner_tx)) {
        CHECK_AND_ASSERT_MES(cumulative_size + 1 == txs_size + get_object_blobsize(b.miner_tx), false, "unexpected case: cumulative_size=" << cumulative_size << " + 1 is not equal txs_cumulative_size=" << txs_size << " + get_object_blobsize(b.miner_tx)=" << get_object_blobsize(b.miner_tx));
        b.miner_tx.extra.resize(b.miner_tx.extra.size() - 1);
        if (cumulative_size != txs_size + get_object_blobsize(b.miner_tx)) {
          //fuck, not lucky, -1 makes varint-counter size smaller, in that case we continue to grow with cumulative_size
          LOG_PRINT_RED("Miner tx creation have no luck with delta_extra size = " << delta << " and " << delta - 1, LOG_LEVEL_2);
          cumulative_size += delta - 1;
          continue;
        }
        LOG_PRINT_GREEN("Setting extra for block: " << b.miner_tx.extra.size() << ", try_count=" << try_count, LOG_LEVEL_1);
      }
    }
    CHECK_AND_ASSERT_MES(cumulative_size == txs_size + get_object_blobsize(b.miner_tx), false, "unexpected case: cumulative_size=" << cumulative_size << " is not equal txs_cumulative_size=" << txs_size << " + get_object_blobsize(b.miner_tx)=" << get_object_blobsize(b.miner_tx));
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
    LOG_PRINT_L1("Creating block template: miner tx size " << coinbase_blob_size <<
      ", cumulative size " << cumulative_size << " is now good");
#endif
    return true;
  }

  LOG_ERROR("Failed to create_block_template with " << 10 << " tries");
  return false;
}

bool blockchain_storage::complete_timestamps_vector(uint64_t start_top_height, std::vector<uint64_t>& timestamps) {
  if (timestamps.size() >= BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW)
    return true;

  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  size_t need_elements = BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW - timestamps.size();
  CHECK_AND_ASSERT_MES(start_top_height < m_blocks.size(), false, "internal error: passed start_height = " << start_top_height << " not less then m_blocks.size()=" << m_blocks.size());
  size_t stop_offset = start_top_height > need_elements ? start_top_height - need_elements : 0;
  do
  {
    timestamps.push_back(m_blocks[start_top_height].bl.timestamp);
    if (start_top_height == 0)
      break;
    --start_top_height;
  } while (start_top_height != stop_offset);
  return true;
}

bool blockchain_storage::handle_alternative_block(const block& b, const crypto::hash& id, block_verification_context& bvc) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  uint64_t block_height = get_block_height(b);
  if (block_height == 0) {
    LOG_ERROR("Block with id: " << epee::string_tools::pod_to_hex(id) << " (as alternative) have wrong miner transaction");
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (!m_checkpoints.is_alternative_block_allowed(get_current_blockchain_height(), block_height)) {
    LOG_PRINT_RED_L0("Block with id: " << id
      << ENDL << " can't be accepted for alternative chain, block height: " << block_height
      << ENDL << " blockchain height: " << get_current_blockchain_height());
    bvc.m_verifivation_failed = true;
    return false;
  }

  //block is not related with head of main chain
  //first of all - look in alternative chains container
  auto it_main_prev = m_blockMap.find(b.prev_id);
  auto it_prev = m_alternative_chains.find(b.prev_id);
  if (it_prev != m_alternative_chains.end() || it_main_prev != m_blockMap.end()) {
    //we have new block in alternative chain

    //build alternative subchain, front -> mainchain, back -> alternative head
    blocks_ext_by_hash::iterator alt_it = it_prev; //m_alternative_chains.find()
    std::list<blocks_ext_by_hash::iterator> alt_chain;
    std::vector<uint64_t> timestamps;
    while (alt_it != m_alternative_chains.end()) {
      alt_chain.push_front(alt_it);
      timestamps.push_back(alt_it->second.bl.timestamp);
      alt_it = m_alternative_chains.find(alt_it->second.bl.prev_id);
    }

    if (alt_chain.size()) {
      //make sure that it has right connection to main chain
      CHECK_AND_ASSERT_MES(m_blocks.size() > alt_chain.front()->second.height, false, "main blockchain wrong height");
      crypto::hash h = null_hash;
      get_block_hash(m_blocks[alt_chain.front()->second.height - 1].bl, h);
      CHECK_AND_ASSERT_MES(h == alt_chain.front()->second.bl.prev_id, false, "alternative chain have wrong connection to main chain");
      complete_timestamps_vector(alt_chain.front()->second.height - 1, timestamps);
    } else {
      CHECK_AND_ASSERT_MES(it_main_prev != m_blockMap.end(), false, "internal error: broken imperative condition it_main_prev != m_blocks_index.end()");
      complete_timestamps_vector(it_main_prev->second, timestamps);
    }

    //check timestamp correct
    if (!check_block_timestamp(timestamps, b)) {
      LOG_PRINT_RED_L0("Block with id: " << id
        << ENDL << " for alternative chain, have invalid timestamp: " << b.timestamp);
      //add_block_as_invalid(b, id);//do not add blocks to invalid storage before proof of work check was passed
      bvc.m_verifivation_failed = true;
      return false;
    }

    Block bei = boost::value_initialized<Block>();
    bei.bl = b;
    bei.height = alt_chain.size() ? it_prev->second.height + 1 : it_main_prev->second + 1;

    bool is_a_checkpoint;
    if (!m_checkpoints.check_block(bei.height, id, is_a_checkpoint)) {
      LOG_ERROR("CHECKPOINT VALIDATION FAILED");
      bvc.m_verifivation_failed = true;
      return false;
    }

    // Always check PoW for alternative blocks
    m_is_in_checkpoint_zone = false;
    difficulty_type current_diff = get_next_difficulty_for_alternative_chain(alt_chain, bei);
    CHECK_AND_ASSERT_MES(current_diff, false, "!!!!!!! DIFFICULTY OVERHEAD !!!!!!!");
    crypto::hash proof_of_work = null_hash;
    get_block_longhash(m_cn_context, bei.bl, proof_of_work, bei.height);
    if (!check_hash(proof_of_work, current_diff)) {
      LOG_PRINT_RED_L0("Block with id: " << id
        << ENDL << " for alternative chain, have not enough proof of work: " << proof_of_work
        << ENDL << " expected difficulty: " << current_diff);
      bvc.m_verifivation_failed = true;
      return false;
    }

    if (!prevalidate_miner_transaction(b, bei.height)) {
      LOG_PRINT_RED_L0("Block with id: " << epee::string_tools::pod_to_hex(id) << " (as alternative) have wrong miner transaction.");
      bvc.m_verifivation_failed = true;
      return false;
    }

    bei.cumulative_difficulty = alt_chain.size() ? it_prev->second.cumulative_difficulty : m_blocks[it_main_prev->second].cumulative_difficulty;
    bei.cumulative_difficulty += current_diff;

#ifdef _DEBUG
    auto i_dres = m_alternative_chains.find(id);
    CHECK_AND_ASSERT_MES(i_dres == m_alternative_chains.end(), false, "insertion of new alternative block returned as it already exist");
#endif

    auto i_res = m_alternative_chains.insert(blocks_ext_by_hash::value_type(id, bei));
    CHECK_AND_ASSERT_MES(i_res.second, false, "insertion of new alternative block returned as it already exist");
    alt_chain.push_back(i_res.first);

    if (is_a_checkpoint) {
      //do reorganize!
      LOG_PRINT_GREEN("###### REORGANIZE on height: " << alt_chain.front()->second.height << " of " << m_blocks.size() - 1 <<
        ", checkpoint is found in alternative chain on height " << bei.height, LOG_LEVEL_0);
      bool r = switch_to_alternative_blockchain(alt_chain, true);
      if (r) bvc.m_added_to_main_chain = true;
      else bvc.m_verifivation_failed = true;
      return r;
    } else if (m_blocks.back().cumulative_difficulty < bei.cumulative_difficulty) //check if difficulty bigger then in main chain
    {
      //do reorganize!
      LOG_PRINT_GREEN("###### REORGANIZE on height: " << alt_chain.front()->second.height << " of " << m_blocks.size() - 1 << " with cum_difficulty " << m_blocks.back().cumulative_difficulty
        << ENDL << " alternative blockchain size: " << alt_chain.size() << " with cum_difficulty " << bei.cumulative_difficulty, LOG_LEVEL_0);
      bool r = switch_to_alternative_blockchain(alt_chain, false);
      if (r) bvc.m_added_to_main_chain = true;
      else bvc.m_verifivation_failed = true;
      return r;
    } else {
      LOG_PRINT_BLUE("----- BLOCK ADDED AS ALTERNATIVE ON HEIGHT " << bei.height
        << ENDL << "id:\t" << id
        << ENDL << "PoW:\t" << proof_of_work
        << ENDL << "difficulty:\t" << current_diff, LOG_LEVEL_0);
      return true;
    }
  } else {
    //block orphaned
    bvc.m_marked_as_orphaned = true;
    LOG_PRINT_RED_L0("Block recognized as orphaned and rejected, id = " << id);
  }

  return true;
}

bool blockchain_storage::get_blocks(uint64_t start_offset, size_t count, std::list<block>& blocks, std::list<transaction>& txs) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (start_offset >= m_blocks.size())
    return false;
  for (size_t i = start_offset; i < start_offset + count && i < m_blocks.size(); i++)
  {
    blocks.push_back(m_blocks[i].bl);
    std::list<crypto::hash> missed_ids;
    get_transactions(m_blocks[i].bl.tx_hashes, txs, missed_ids);
    CHECK_AND_ASSERT_MES(!missed_ids.size(), false, "have missed transactions in own block in main blockchain");
  }

  return true;
}

bool blockchain_storage::get_blocks(uint64_t start_offset, size_t count, std::list<block>& blocks) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (start_offset >= m_blocks.size()) {
    return false;
  }

  for (size_t i = start_offset; i < start_offset + count && i < m_blocks.size(); i++) {
    blocks.push_back(m_blocks[i].bl);
  }

  return true;
}

bool blockchain_storage::handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  rsp.current_blockchain_height = get_current_blockchain_height();
  std::list<block> blocks;
  get_blocks(arg.blocks, blocks, rsp.missed_ids);

  for (const auto& bl : blocks) {
    std::list<crypto::hash> missed_tx_id;
    std::list<transaction> txs;
    get_transactions(bl.tx_hashes, txs, rsp.missed_ids);
    CHECK_AND_ASSERT_MES(!missed_tx_id.size(), false, "Internal error: have missed missed_tx_id.size()=" << missed_tx_id.size() << ENDL << "for block id = " << get_block_hash(bl));
    rsp.blocks.push_back(block_complete_entry());
    block_complete_entry& e = rsp.blocks.back();
    //pack block
    e.block = t_serializable_object_to_blob(bl);
    //pack transactions
    for (transaction& tx : txs) {
      e.txs.push_back(t_serializable_object_to_blob(tx));
    }
  }

  //get another transactions, if need
  std::list<transaction> txs;
  get_transactions(arg.txs, txs, rsp.missed_ids);
  //pack aside transactions
  for (const auto& tx : txs) {
    rsp.txs.push_back(t_serializable_object_to_blob(tx));
  }

  return true;
}

bool blockchain_storage::get_alternative_blocks(std::list<block>& blocks) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  for (auto& alt_bl : m_alternative_chains) {
    blocks.push_back(alt_bl.second.bl);
  }

  return true;
}

size_t blockchain_storage::get_alternative_blocks_count() {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_alternative_chains.size();
}

bool blockchain_storage::add_out_to_get_random_outs(std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs, uint64_t amount, size_t i) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  const transaction& tx = transactionByIndex(amount_outs[i].first).tx;
  CHECK_AND_ASSERT_MES(tx.vout.size() > amount_outs[i].second, false, "internal error: in global outs index, transaction out index="
    << amount_outs[i].second << " more than transaction outputs = " << tx.vout.size() << ", for tx id = " << get_transaction_hash(tx));
  CHECK_AND_ASSERT_MES(tx.vout[amount_outs[i].second].target.type() == typeid(txout_to_key), false, "unknown tx out type");

  //check if transaction is unlocked
  if (!is_tx_spendtime_unlocked(tx.unlock_time))
    return false;

  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& oen = *result_outs.outs.insert(result_outs.outs.end(), COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry());
  oen.global_amount_index = i;
  oen.out_key = boost::get<txout_to_key>(tx.vout[amount_outs[i].second].target).key;
  return true;
}

size_t blockchain_storage::find_end_of_allowed_index(const std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (amount_outs.empty()) {
    return 0;
  }

  size_t i = amount_outs.size();
  do {
    --i;
    if (amount_outs[i].first.block + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW <= get_current_blockchain_height()) {
      return i + 1;
    }
  } while (i != 0);

  return 0;
}

bool blockchain_storage::get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) {
  srand(static_cast<unsigned int>(time(NULL)));
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  for (uint64_t amount : req.amounts) {
    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs = *res.outs.insert(res.outs.end(), COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount());
    result_outs.amount = amount;
    auto it = m_outputs.find(amount);
    if (it == m_outputs.end()) {
      LOG_ERROR("COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS: not outs for amount " << amount << ", wallet should use some real outs when it lookup for some mix, so, at least one out for this amount should exist");
      continue;//actually this is strange situation, wallet should use some real outs when it lookup for some mix, so, at least one out for this amount should exist
    }

    std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs = it->second;
    //it is not good idea to use top fresh outs, because it increases possibility of transaction canceling on split
    //lets find upper bound of not fresh outs
    size_t up_index_limit = find_end_of_allowed_index(amount_outs);
    CHECK_AND_ASSERT_MES(up_index_limit <= amount_outs.size(), false, "internal error: find_end_of_allowed_index returned wrong index=" << up_index_limit << ", with amount_outs.size = " << amount_outs.size());
    if (amount_outs.size() > req.outs_count) {
      std::set<size_t> used;
      size_t try_count = 0;
      for (uint64_t j = 0; j != req.outs_count && try_count < up_index_limit;) {
        size_t i = rand() % up_index_limit;
        if (used.count(i))
          continue;
        bool added = add_out_to_get_random_outs(amount_outs, result_outs, amount, i);
        used.insert(i);
        if (added)
          ++j;
        ++try_count;
      }
    } else {
      for (size_t i = 0; i != up_index_limit; i++) {
        add_out_to_get_random_outs(amount_outs, result_outs, amount, i);
      }
    }
  }
  return true;
}

bool blockchain_storage::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, uint64_t& starter_offset)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  if (!qblock_ids.size() /*|| !req.m_total_height*/)
  {
    LOG_ERROR("Client sent wrong NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << qblock_ids.size() << /*", m_height=" << req.m_total_height <<*/ ", dropping connection");
    return false;
  }
  //check genesis match
  if (qblock_ids.back() != get_block_hash(m_blocks[0].bl))
  {
    LOG_ERROR("Client sent wrong NOTIFY_REQUEST_CHAIN: genesis block missmatch: " << ENDL << "id: "
      << qblock_ids.back() << ", " << ENDL << "expected: " << get_block_hash(m_blocks[0].bl)
      << "," << ENDL << " dropping connection");
    return false;
  }

  /* Figure out what blocks we should request to get state_normal */
  size_t i = 0;
  auto bl_it = qblock_ids.begin();
  auto block_index_it = m_blockMap.find(*bl_it);
  for (; bl_it != qblock_ids.end(); bl_it++, i++)
  {
    block_index_it = m_blockMap.find(*bl_it);
    if (block_index_it != m_blockMap.end())
      break;
  }

  if (bl_it == qblock_ids.end())
  {
    LOG_ERROR("Internal error handling connection, can't find split point");
    return false;
  }

  if (block_index_it == m_blockMap.end())
  {
    //this should NEVER happen, but, dose of paranoia in such cases is not too bad
    LOG_ERROR("Internal error handling connection, can't find split point");
    return false;
  }

  //we start to put block ids INCLUDING last known id, just to make other side be sure
  starter_offset = block_index_it->second;
  return true;
}

uint64_t blockchain_storage::block_difficulty(size_t i)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  CHECK_AND_ASSERT_MES(i < m_blocks.size(), false, "wrong block index i = " << i << " at blockchain_storage::block_difficulty()");
  if (i == 0)
    return m_blocks[i].cumulative_difficulty;

  return m_blocks[i].cumulative_difficulty - m_blocks[i - 1].cumulative_difficulty;
}

void blockchain_storage::print_blockchain(uint64_t start_index, uint64_t end_index)
{
  std::stringstream ss;
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (start_index >= m_blocks.size())
  {
    LOG_PRINT_L0("Wrong starter index set: " << start_index << ", expected max index " << m_blocks.size() - 1);
    return;
  }

  for (size_t i = start_index; i != m_blocks.size() && i != end_index; i++)
  {
    ss << "height " << i << ", timestamp " << m_blocks[i].bl.timestamp << ", cumul_dif " << m_blocks[i].cumulative_difficulty << ", cumul_size " << m_blocks[i].block_cumulative_size
      << "\nid\t\t" << get_block_hash(m_blocks[i].bl)
      << "\ndifficulty\t\t" << block_difficulty(i) << ", nonce " << m_blocks[i].bl.nonce << ", tx_count " << m_blocks[i].bl.tx_hashes.size() << ENDL;
  }
  LOG_PRINT_L1("Current blockchain:" << ENDL << ss.str());
  LOG_PRINT_L0("Blockchain printed with log level 1");
}

void blockchain_storage::print_blockchain_index() {
  std::stringstream ss;
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  for (auto& i : m_blockMap) {
    ss << "id\t\t" << i.first << " height" << i.second << ENDL << "";
  }

  LOG_PRINT_L0("Current blockchain index:" << ENDL << ss.str());
}

void blockchain_storage::print_blockchain_outs(const std::string& file) {
  std::stringstream ss;
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  for (const outputs_container::value_type& v : m_outputs) {
    const std::vector<std::pair<TransactionIndex, uint16_t>>& vals = v.second;
    if (!vals.empty()) {
      ss << "amount: " << v.first << ENDL;
      for (size_t i = 0; i != vals.size(); i++) {
        ss << "\t" << get_transaction_hash(transactionByIndex(vals[i].first).tx) << ": " << vals[i].second << ENDL;
      }
    }
  }

  if (epee::file_io_utils::save_string_to_file(file, ss.str())) {
    LOG_PRINT_L0("Current outputs index writen to file: " << file);
  } else {
    LOG_PRINT_L0("Failed to write current outputs index to file: " << file);
  }
}

bool blockchain_storage::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!find_blockchain_supplement(qblock_ids, resp.start_height))
    return false;

  resp.total_height = get_current_blockchain_height();
  size_t count = 0;
  for (size_t i = resp.start_height; i != m_blocks.size() && count < BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT; i++, count++) {
    crypto::hash h;
    if (!get_block_hash(m_blocks[i].bl, h)) {
      return false;
    }

    resp.m_block_ids.push_back(h);
  }

  return true;
}

bool blockchain_storage::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<block, std::list<transaction> > >& blocks, uint64_t& total_height, uint64_t& start_height, size_t max_count) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!find_blockchain_supplement(qblock_ids, start_height)) {
    return false;
  }

  total_height = get_current_blockchain_height();
  size_t count = 0;
  for (size_t i = start_height; i != m_blocks.size() && count < max_count; i++, count++) {
    blocks.resize(blocks.size() + 1);
    blocks.back().first = m_blocks[i].bl;
    std::list<crypto::hash> mis;
    get_transactions(m_blocks[i].bl.tx_hashes, blocks.back().second, mis);
    CHECK_AND_ASSERT_MES(!mis.size(), false, "internal error, transaction from block not found");
  }

  return true;
}

bool blockchain_storage::have_block(const crypto::hash& id)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (m_blockMap.count(id))
    return true;

  if (m_alternative_chains.count(id))
    return true;

  return false;
}

size_t blockchain_storage::get_total_transactions() {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_transactionMap.size();
}

bool blockchain_storage::get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  auto it = m_transactionMap.find(tx_id);
  if (it == m_transactionMap.end()) {
    LOG_PRINT_RED_L0("warning: get_tx_outputs_gindexs failed to find transaction with id = " << tx_id);
    return false;
  }

  const Transaction& tx = transactionByIndex(it->second);
  CHECK_AND_ASSERT_MES(tx.m_global_output_indexes.size(), false, "internal error: global indexes for transaction " << tx_id << " is empty");
  indexs.resize(tx.m_global_output_indexes.size());
  for (size_t i = 0; i < tx.m_global_output_indexes.size(); ++i) {
    indexs[i] = tx.m_global_output_indexes[i];
  }

  return true;
}

bool blockchain_storage::check_tx_inputs(const transaction& tx, uint64_t& max_used_block_height, crypto::hash& max_used_block_id) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  bool res = check_tx_inputs(tx, &max_used_block_height);
  if (!res) return false;
  CHECK_AND_ASSERT_MES(max_used_block_height < m_blocks.size(), false, "internal error: max used block index=" << max_used_block_height << " is not less then blockchain size = " << m_blocks.size());
  get_block_hash(m_blocks[max_used_block_height].bl, max_used_block_id);
  return true;
}

bool blockchain_storage::have_tx_keyimges_as_spent(const transaction &tx) {
  for(const txin_v& in : tx.vin) {
    CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, in_to_key, true);
    if (have_tx_keyimg_as_spent(in_to_key.k_image)) {
      return true;
    }
  }

  return false;
}

bool blockchain_storage::check_tx_inputs(const transaction& tx, uint64_t* pmax_used_block_height) {
  crypto::hash tx_prefix_hash = get_transaction_prefix_hash(tx);
  return check_tx_inputs(tx, tx_prefix_hash, pmax_used_block_height);
}

bool blockchain_storage::check_tx_inputs(const transaction& tx, const crypto::hash& tx_prefix_hash, uint64_t* pmax_used_block_height) {
  size_t sig_index = 0;
  if (pmax_used_block_height) {
    *pmax_used_block_height = 0;
  }

  for (const auto& txin : tx.vin) {
    CHECK_AND_ASSERT_MES(txin.type() == typeid(txin_to_key), false, "wrong type id in tx input at blockchain_storage::check_tx_inputs");
    const txin_to_key& in_to_key = boost::get<txin_to_key>(txin);

    CHECK_AND_ASSERT_MES(in_to_key.key_offsets.size(), false, "empty in_to_key.key_offsets in transaction with id " << get_transaction_hash(tx));

    if (have_tx_keyimg_as_spent(in_to_key.k_image))
    {
      LOG_PRINT_L1("Key image already spent in blockchain: " << epee::string_tools::pod_to_hex(in_to_key.k_image));
      return false;
    }

    CHECK_AND_ASSERT_MES(sig_index < tx.signatures.size(), false, "wrong transaction: not signature entry for input with index= " << sig_index);
    if (!check_tx_input(in_to_key, tx_prefix_hash, tx.signatures[sig_index], pmax_used_block_height)) {
      LOG_PRINT_L0("Failed to check ring signature for tx " << get_transaction_hash(tx));
      return false;
    }

    sig_index++;
  }

  return true;
}

bool blockchain_storage::is_tx_spendtime_unlocked(uint64_t unlock_time) {
  if (unlock_time < CRYPTONOTE_MAX_BLOCK_NUMBER) {
    //interpret as block index
    if (get_current_blockchain_height() - 1 + CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS >= unlock_time)
      return true;
    else
      return false;
  } else {
    //interpret as time
    uint64_t current_time = static_cast<uint64_t>(time(NULL));
    if (current_time + CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS >= unlock_time)
      return true;
    else
      return false;
  }

  return false;
}

bool blockchain_storage::check_tx_input(const txin_to_key& txin, const crypto::hash& tx_prefix_hash, const std::vector<crypto::signature>& sig, uint64_t* pmax_related_block_height) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  struct outputs_visitor
  {
    std::vector<const crypto::public_key *>& m_results_collector;
    blockchain_storage& m_bch;
    outputs_visitor(std::vector<const crypto::public_key *>& results_collector, blockchain_storage& bch) :m_results_collector(results_collector), m_bch(bch)
    {}
    bool handle_output(const transaction& tx, const tx_out& out) {
      //check tx unlock time
      if (!m_bch.is_tx_spendtime_unlocked(tx.unlock_time)) {
        LOG_PRINT_L0("One of outputs for one of inputs have wrong tx.unlock_time = " << tx.unlock_time);
        return false;
      }

      if (out.target.type() != typeid(txout_to_key))
      {
        LOG_PRINT_L0("Output have wrong type id, which=" << out.target.which());
        return false;
      }

      m_results_collector.push_back(&boost::get<txout_to_key>(out.target).key);
      return true;
    }
  };

  //check ring signature
  std::vector<const crypto::public_key *> output_keys;
  outputs_visitor vi(output_keys, *this);
  if (!scan_outputkeys_for_indexes(txin, vi, pmax_related_block_height)) {
    LOG_PRINT_L0("Failed to get output keys for tx with amount = " << print_money(txin.amount) << " and count indexes " << txin.key_offsets.size());
    return false;
  }

  if (txin.key_offsets.size() != output_keys.size()) {
    LOG_PRINT_L0("Output keys for tx with amount = " << txin.amount << " and count indexes " << txin.key_offsets.size() << " returned wrong keys count " << output_keys.size());
    return false;
  }

  CHECK_AND_ASSERT_MES(sig.size() == output_keys.size(), false, "internal error: tx signatures count=" << sig.size() << " mismatch with outputs keys count for inputs=" << output_keys.size());
  if (m_is_in_checkpoint_zone) {
    return true;
  }

  return crypto::check_ring_signature(tx_prefix_hash, txin.k_image, output_keys, sig.data());
}

uint64_t blockchain_storage::get_adjusted_time() {
  //TODO: add collecting median time
  return time(NULL);
}

bool blockchain_storage::check_block_timestamp_main(const block& b) {
  if (b.timestamp > get_adjusted_time() + CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT) {
    LOG_PRINT_L0("Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp << ", bigger than adjusted time + 2 hours");
    return false;
  }

  std::vector<uint64_t> timestamps;
  size_t offset = m_blocks.size() <= BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW ? 0 : m_blocks.size() - BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW;
  for (; offset != m_blocks.size(); ++offset) {
    timestamps.push_back(m_blocks[offset].bl.timestamp);
  }

  return check_block_timestamp(std::move(timestamps), b);
}

bool blockchain_storage::check_block_timestamp(std::vector<uint64_t> timestamps, const block& b) {
  if (timestamps.size() < BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW) {
    return true;
  }

  uint64_t median_ts = epee::misc_utils::median(timestamps);

  if (b.timestamp < median_ts) {
    LOG_PRINT_L0("Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp << ", less than median of last " << BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW << " blocks, " << median_ts);
    return false;
  }

  return true;
}

bool blockchain_storage::update_next_comulative_size_limit() {
  std::vector<size_t> sz;
  get_last_n_blocks_sizes(sz, CRYPTONOTE_REWARD_BLOCKS_WINDOW);

  uint64_t median = epee::misc_utils::median(sz);
  if (median <= CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE)
    median = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;

  m_current_block_cumul_sz_limit = median * 2;
  return true;
}

bool blockchain_storage::add_new_block(const block& bl_, block_verification_context& bvc) {
  //copy block here to let modify block.target
  block bl = bl_;
  crypto::hash id = get_block_hash(bl);
  CRITICAL_REGION_LOCAL(m_tx_pool);//to avoid deadlock lets lock tx_pool for whole add/reorganize process
  CRITICAL_REGION_LOCAL1(m_blockchain_lock);
  if (have_block(id)) {
    LOG_PRINT_L3("block with id = " << id << " already exists");
    bvc.m_already_exists = true;
    return false;
  }

  //check that block refers to chain tail
  if (!(bl.prev_id == get_tail_id())) {
    //chain switching or wrong block
    bvc.m_added_to_main_chain = false;
    return handle_alternative_block(bl, id, bvc);
    //never relay alternative blocks
  }

  return pushBlock(bl, bvc);
}

const blockchain_storage::Transaction& blockchain_storage::transactionByIndex(TransactionIndex index) {
  return m_blocks[index.block].transactions[index.transaction];
}

bool blockchain_storage::pushBlock(const block& blockData, block_verification_context& bvc) {
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  TIME_MEASURE_START(block_processing_time);

  crypto::hash blockHash = get_block_hash(blockData);

  if (m_blockMap.count(blockHash) != 0) {
    LOG_ERROR("Block " << blockHash << " already exists in blockchain.");
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (blockData.prev_id != get_tail_id()) {
    LOG_PRINT_L0("Block " << blockHash << " has wrong prev_id: " << blockData.prev_id << ", expected: " << get_tail_id());
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (!check_block_timestamp_main(blockData)) {
    LOG_PRINT_L0("Block " << blockHash << " has invalid timestamp: " << blockData.timestamp);
    bvc.m_verifivation_failed = true;
    return false;
  }

  TIME_MEASURE_START(target_calculating_time);
  difficulty_type currentDifficulty = get_difficulty_for_next_block();
  TIME_MEASURE_FINISH(target_calculating_time);
  CHECK_AND_ASSERT_MES(currentDifficulty, false, "!!!!!!!!! difficulty overhead !!!!!!!!!");

  TIME_MEASURE_START(longhash_calculating_time);
  crypto::hash proof_of_work = null_hash;
  if (m_checkpoints.is_in_checkpoint_zone(get_current_blockchain_height())) {
    if (!m_checkpoints.check_block(get_current_blockchain_height(), blockHash)) {
      LOG_ERROR("CHECKPOINT VALIDATION FAILED");
      bvc.m_verifivation_failed = true;
      return false;
    }
  } else {
    proof_of_work = get_block_longhash(m_cn_context, blockData, m_blocks.size());
    if (!check_hash(proof_of_work, currentDifficulty)) {
      LOG_PRINT_L0("Block " << blockHash << ", has too weak proof of work: " << proof_of_work << ", expected difficulty: " << currentDifficulty);
      bvc.m_verifivation_failed = true;
      return false;
    }
  }

  TIME_MEASURE_FINISH(longhash_calculating_time);

  if (!prevalidate_miner_transaction(blockData, m_blocks.size())) {
    LOG_PRINT_L0("Block " << blockHash << " failed to pass prevalidation");
    bvc.m_verifivation_failed = true;
    return false;
  }

  crypto::hash minerTransactionHash = get_transaction_hash(blockData.miner_tx);

  Block block;
  block.bl = blockData;
  block.transactions.resize(1);
  block.transactions[0].tx =  blockData.miner_tx;
  TransactionIndex transactionIndex = { static_cast<uint32_t>(m_blocks.size()), static_cast<uint16_t>(0) };
  pushTransaction(block, minerTransactionHash, transactionIndex);

  size_t coinbase_blob_size = get_object_blobsize(blockData.miner_tx);
  size_t cumulative_block_size = coinbase_blob_size;
  uint64_t fee_summary = 0;
  for (const crypto::hash& tx_id : blockData.tx_hashes) {
    block.transactions.resize(block.transactions.size() + 1);
    size_t blob_size = 0;
    uint64_t fee = 0;
    if (!m_tx_pool.take_tx(tx_id, block.transactions.back().tx, blob_size, fee)) {
      LOG_PRINT_L0("Block " << blockHash << " has at least one unknown transaction: " << tx_id);
      bvc.m_verifivation_failed = true;
      tx_verification_context tvc = ::AUTO_VAL_INIT(tvc);
      block.transactions.pop_back();
      popTransactions(block, minerTransactionHash);
      return false;
    }

    if (!check_tx_inputs(block.transactions.back().tx)) {
      LOG_PRINT_L0("Block " << blockHash << " has at least one transaction with wrong inputs: " << tx_id);
      bvc.m_verifivation_failed = true;
      tx_verification_context tvc = ::AUTO_VAL_INIT(tvc);
      if (!m_tx_pool.add_tx(block.transactions.back().tx, tvc, true)) {
        LOG_ERROR("Cannot move transaction from blockchain to transaction pool.");
      }

      block.transactions.pop_back();
      popTransactions(block, minerTransactionHash);
      return false;
    }

    ++transactionIndex.transaction;
    pushTransaction(block, tx_id, transactionIndex);

    cumulative_block_size += blob_size;
    fee_summary += fee;
  }

  uint64_t base_reward = 0;
  uint64_t already_generated_coins = m_blocks.size() ? m_blocks.back().already_generated_coins : 0;
  if (!validate_miner_transaction(blockData, cumulative_block_size, fee_summary, base_reward, already_generated_coins)) {
    LOG_PRINT_L0("Block " << blockHash << " has invalid miner transaction");
    bvc.m_verifivation_failed = true;
    popTransactions(block, minerTransactionHash);
    return false;
  }

  block.height = static_cast<uint32_t>(m_blocks.size());
  block.block_cumulative_size = cumulative_block_size;
  block.cumulative_difficulty = currentDifficulty;
  block.already_generated_coins = already_generated_coins + base_reward;
  if (m_blocks.size() > 0) {
    block.cumulative_difficulty += m_blocks.back().cumulative_difficulty;
  }

  pushBlock(block);
  update_next_comulative_size_limit();
  TIME_MEASURE_FINISH(block_processing_time);
  LOG_PRINT_L1("+++++ BLOCK SUCCESSFULLY ADDED" << ENDL << "id:\t" << blockHash
    << ENDL << "PoW:\t" << proof_of_work
    << ENDL << "HEIGHT " << block.height << ", difficulty:\t" << currentDifficulty
    << ENDL << "block reward: " << print_money(fee_summary + base_reward) << "(" << print_money(base_reward) << " + " << print_money(fee_summary)
    << "), coinbase_blob_size: " << coinbase_blob_size << ", cumulative size: " << cumulative_block_size
    << ", " << block_processing_time << "(" << target_calculating_time << "/" << longhash_calculating_time << ")ms");

  bvc.m_added_to_main_chain = true;
  return true;
}

bool blockchain_storage::pushBlock(Block& block) {
  crypto::hash blockHash = get_block_hash(block.bl);
  auto result = m_blockMap.insert(std::make_pair(blockHash, static_cast<uint32_t>(m_blocks.size())));
  if (!result.second) {
    LOG_ERROR("Duplicate block was pushed to blockchain.");
    return false;
  }

  m_blocks.push_back(block);
  return true;
}

void blockchain_storage::popBlock(const crypto::hash& blockHash) {
  if (m_blocks.empty()) {
    LOG_ERROR("Attempt to pop block from empty blockchain.");
    return;
  }

  popTransactions(m_blocks.back(), get_transaction_hash(m_blocks.back().bl.miner_tx));
  m_blocks.pop_back();
  size_t count = m_blockMap.erase(blockHash);
  if (count != 1) {
    LOG_ERROR("Blockchain consistency broken - cannot find block by hash.");
  }
}

bool blockchain_storage::pushTransaction(Block& block, const crypto::hash& transactionHash, TransactionIndex transactionIndex) {
  auto result = m_transactionMap.insert(std::make_pair(transactionHash, transactionIndex));
  if (!result.second) {
    LOG_ERROR("Duplicate transaction was pushed to blockchain.");
    return false;
  }

  Transaction& transaction = block.transactions[transactionIndex.transaction];
  for (size_t i = 0; i < transaction.tx.vin.size(); ++i) {
    if (transaction.tx.vin[i].type() == typeid(txin_to_key)) {
      auto result = m_spent_keys.insert(::boost::get<txin_to_key>(transaction.tx.vin[i]).k_image);
      if (!result.second) {
        LOG_ERROR("Double spending transaction was pushed to blockchain.");
        for (size_t j = 0; j < i; ++j) {
          m_spent_keys.erase(::boost::get<txin_to_key>(transaction.tx.vin[i - 1 - j]).k_image);
        }

        m_transactionMap.erase(transactionHash);
        return false;
      }
    }
  }

  transaction.m_global_output_indexes.resize(transaction.tx.vout.size());
  for (uint16_t output = 0; output < transaction.tx.vout.size(); ++output) {
    auto& amountOutputs = m_outputs[transaction.tx.vout[output].amount];
    transaction.m_global_output_indexes[output] = amountOutputs.size();
    amountOutputs.push_back(std::make_pair<>(transactionIndex, output));
  }

  return true;
}

void blockchain_storage::popTransaction(const transaction& transaction, const crypto::hash& transactionHash) {
  TransactionIndex transactionIndex = m_transactionMap.at(transactionHash);
  for (size_t output = 0; output < transaction.vout.size(); ++output) {
    auto amountOutputs = m_outputs.find(transaction.vout[transaction.vout.size() - 1 - output].amount);
    if (amountOutputs == m_outputs.end()) {
      LOG_ERROR("Blockchain consistency broken - cannot find specific amount in outputs map.");
      continue;
    }

    if (amountOutputs->second.empty()) {
      LOG_ERROR("Blockchain consistency broken - output array for specific amount is empty.");
      continue;
    }

    if (amountOutputs->second.back().first.block != transactionIndex.block || amountOutputs->second.back().first.transaction != transactionIndex.transaction) {
      LOG_ERROR("Blockchain consistency broken - invalid transaction index.");
      continue;
    }

    if (amountOutputs->second.back().second != transaction.vout.size() - 1 - output) {
      LOG_ERROR("Blockchain consistency broken - invalid output index.");
      continue;
    }

    amountOutputs->second.pop_back();
    if (amountOutputs->second.empty()) {
      m_outputs.erase(amountOutputs);
    }
  }

  for (auto& input : transaction.vin) {
    if (input.type() == typeid(txin_to_key)) {
      size_t count = m_spent_keys.erase(::boost::get<txin_to_key>(input).k_image);
      if (count != 1) {
        LOG_ERROR("Blockchain consistency broken - cannot find spent key.");
      }
    }
  }

  size_t count = m_transactionMap.erase(transactionHash);
  if (count != 1) {
    LOG_ERROR("Blockchain consistency broken - cannot find transaction by hash.");
  }
}

void blockchain_storage::popTransactions(const Block& block, const crypto::hash& minerTransactionHash) {
  for (size_t i = 0; i < block.transactions.size() - 1; ++i) {
    popTransaction(block.transactions[block.transactions.size() - 1 - i].tx, block.bl.tx_hashes[block.transactions.size() - 2 - i]);
    tx_verification_context tvc = ::AUTO_VAL_INIT(tvc);
    if (!m_tx_pool.add_tx(block.transactions[block.transactions.size() - 1 - i].tx, tvc, true)) {
      LOG_ERROR("Cannot move transaction from blockchain to transaction pool.");
    }
  }

  popTransaction(block.bl.miner_tx, minerTransactionHash);
}
