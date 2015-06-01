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

#include "blockchain_storage.h"

#include <algorithm>
#include <cstdio>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/foreach.hpp>
#include <boost/serialization/version.hpp>
#include <boost/utility/value_init.hpp>

#include "Common/boost_serialization_helper.h"
#include "Common/ShuffleGenerator.h"
#include "Common/StringTools.h"

#include "cryptonote_format_utils.h"
#include "rpc/core_rpc_server_commands_defs.h"

using namespace Logging;

#ifdef ENDL
#undef ENDL
#define ENDL '\n'
#endif

namespace {
std::string appendPath(const std::string& path, const std::string& fileName) {
  std::string result = path;
  if (!result.empty()) {
    result += '/';
  }

  result += fileName;
  return result;
}

template<class type_vec_type>
type_vec_type medianValue(std::vector<type_vec_type> &v)
{
  if (v.empty())
    return boost::value_initialized<type_vec_type>();

  if (v.size() == 1)
    return v[0];

  size_t n = (v.size()) / 2;
  std::sort(v.begin(), v.end());
  //nth_element(v.begin(), v.begin()+n-1, v.end());
  if (v.size() % 2)
  {//1, 3, 5...
    return v[n];
  } else
  {//2, 4, 6...
    return (v[n - 1] + v[n]) / 2;
  }
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

#define CURRENT_BLOCKCACHE_STORAGE_ARCHIVE_VER 1

namespace CryptoNote {
class BlockCacheSerializer;
}

BOOST_CLASS_VERSION(CryptoNote::BlockCacheSerializer, CURRENT_BLOCKCACHE_STORAGE_ARCHIVE_VER);

namespace CryptoNote
{

template<class Archive> 
void blockchain_storage::TransactionIndex::serialize(Archive& archive, unsigned int version) {
  archive & block;
  archive & transaction;
}

template<class Archive> 
void blockchain_storage::MultisignatureOutputUsage::serialize(Archive& archive, unsigned int version) {
  archive & transactionIndex;
  archive & outputIndex;
  archive & isUsed;
}

class BlockCacheSerializer {

public:
  BlockCacheSerializer(blockchain_storage& bs, const crypto::hash lastBlockHash, ILogger& logger) :
    m_bs(bs), m_lastBlockHash(lastBlockHash), m_loaded(false), logger(logger, "BlockCacheSerializer") {
  }

  template<class Archive> void serialize(Archive& ar, unsigned int version) {

    // ignore old versions, do rebuild
    if (version < CURRENT_BLOCKCACHE_STORAGE_ARCHIVE_VER)
      return;

    std::string operation;
    if (Archive::is_loading::value) {
      operation = "- loading ";
      crypto::hash blockHash;
      ar & blockHash;

      if (blockHash != m_lastBlockHash) {
        return;
      }

    } else {
      operation = "- saving ";
      ar & m_lastBlockHash;
    }

    logger(INFO) << operation << "block index...";
    ar & m_bs.m_blockIndex;

    logger(INFO) << operation << "transaction map...";
    ar & m_bs.m_transactionMap;

    logger(INFO) << operation << "spend keys...";
    ar & m_bs.m_spent_keys;

    logger(INFO) << operation << "outputs...";
    ar & m_bs.m_outputs;

    logger(INFO) << operation << "multi-signature outputs...";
    ar & m_bs.m_multisignatureOutputs;

    m_loaded = true;
  }

  bool loaded() const {
    return m_loaded;
  }

private:

  LoggerRef logger;
  bool m_loaded;
  blockchain_storage& m_bs;
  crypto::hash m_lastBlockHash;
};


blockchain_storage::blockchain_storage(const Currency& currency, tx_memory_pool& tx_pool, ILogger& logger) :
logger(logger, "blockchain_storage"),
m_currency(currency),
m_tx_pool(tx_pool),
m_current_block_cumul_sz_limit(0),
m_is_in_checkpoint_zone(false),
m_is_blockchain_storing(false),
m_upgradeDetector(currency, m_blocks, BLOCK_MAJOR_VERSION_2, logger),
m_checkpoints(logger) {

  m_outputs.set_deleted_key(0);
  crypto::key_image nullImage = boost::value_initialized<decltype(nullImage)>();
  m_spent_keys.set_deleted_key(nullImage);
}

bool blockchain_storage::addObserver(IBlockchainStorageObserver* observer) {
  return m_observerManager.add(observer);
}

bool blockchain_storage::removeObserver(IBlockchainStorageObserver* observer) {
  return m_observerManager.remove(observer);
}

bool blockchain_storage::checkTransactionInputs(const CryptoNote::Transaction& tx, BlockInfo& maxUsedBlock) {
  return check_tx_inputs(tx, maxUsedBlock.height, maxUsedBlock.id);
}

bool blockchain_storage::checkTransactionInputs(const CryptoNote::Transaction& tx, BlockInfo& maxUsedBlock, BlockInfo& lastFailed) {

  BlockInfo tail;

  //not the best implementation at this time, sorry :(
  //check is ring_signature already checked ?
  if (maxUsedBlock.empty()) {
    //not checked, lets try to check
    if (!lastFailed.empty() && get_current_blockchain_height() > lastFailed.height && get_block_id_by_height(lastFailed.height) == lastFailed.id) {
      return false; //we already sure that this tx is broken for this height
    }

    if (!check_tx_inputs(tx, maxUsedBlock.height, maxUsedBlock.id, &tail)) {
      lastFailed = tail;
      return false;
    }
  } else {
    if (maxUsedBlock.height >= get_current_blockchain_height()) {
      return false;
    }

    if (get_block_id_by_height(maxUsedBlock.height) != maxUsedBlock.id) {
      //if we already failed on this height and id, skip actual ring signature check
      if (lastFailed.id == get_block_id_by_height(lastFailed.height)) {
        return false;
      }

      //check ring signature again, it is possible (with very small chance) that this transaction become again valid
      if (!check_tx_inputs(tx, maxUsedBlock.height, maxUsedBlock.id, &tail)) {
        lastFailed = tail;
        return false;
      }
    }
  }

  return true;
}

bool blockchain_storage::haveSpentKeyImages(const CryptoNote::Transaction& tx) {
  return this->have_tx_keyimges_as_spent(tx);
}

bool blockchain_storage::have_tx(const crypto::hash &id) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_transactionMap.find(id) != m_transactionMap.end();
}

bool blockchain_storage::have_tx_keyimg_as_spent(const crypto::key_image &key_im) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return  m_spent_keys.find(key_im) != m_spent_keys.end();
}

uint64_t blockchain_storage::get_current_blockchain_height() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_blocks.size();
}

bool blockchain_storage::init(const std::string& config_folder, bool load_existing) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!config_folder.empty() && !tools::create_directories_if_necessary(config_folder)) {
    logger(ERROR, BRIGHT_RED) << "Failed to create data directory: " << m_config_folder;
    return false;
  }

  m_config_folder = config_folder;

  if (!m_blocks.open(appendPath(config_folder, m_currency.blocksFileName()), appendPath(config_folder, m_currency.blockIndexesFileName()), 1024)) {
    return false;
  }

  if (load_existing && !m_blocks.empty()) {
    logger(INFO, BRIGHT_WHITE) << "Loading blockchain...";
    BlockCacheSerializer loader(*this, get_block_hash(m_blocks.back().bl), logger.getLogger());
    tools::unserialize_obj_from_file(loader, appendPath(config_folder, m_currency.blocksCacheFileName()));

    if (!loader.loaded()) {
      logger(WARNING, BRIGHT_YELLOW) << "No actual blockchain cache found, rebuilding internal structures...";
      std::chrono::steady_clock::time_point timePoint = std::chrono::steady_clock::now();
      m_blockIndex.clear();
      m_transactionMap.clear();
      m_spent_keys.clear();
      m_outputs.clear();
      m_multisignatureOutputs.clear();
      for (uint32_t b = 0; b < m_blocks.size(); ++b) {
        if (b % 1000 == 0) {
          logger(INFO, BRIGHT_WHITE) << "Height " << b << " of " << m_blocks.size();
        }
        const BlockEntry& block = m_blocks[b];
        crypto::hash blockHash = get_block_hash(block.bl);
        m_blockIndex.push(blockHash);
        for (uint16_t t = 0; t < block.transactions.size(); ++t) {
          const TransactionEntry& transaction = block.transactions[t];
          crypto::hash transactionHash = get_transaction_hash(transaction.tx);
          TransactionIndex transactionIndex = {b, t};
          m_transactionMap.insert(std::make_pair(transactionHash, transactionIndex));

          // process inputs
          for (auto& i : transaction.tx.vin) {
            if (i.type() == typeid(TransactionInputToKey)) {
              m_spent_keys.insert(::boost::get<TransactionInputToKey>(i).keyImage);
            } else if (i.type() == typeid(TransactionInputMultisignature)) {
              auto out = ::boost::get<TransactionInputMultisignature>(i);
              m_multisignatureOutputs[out.amount][out.outputIndex].isUsed = true;
            }
          }

          // process outputs
          for (uint16_t o = 0; o < transaction.tx.vout.size(); ++o) {
            const auto& out = transaction.tx.vout[o];
            if (out.target.type() == typeid(TransactionOutputToKey)) {
              m_outputs[out.amount].push_back(std::make_pair<>(transactionIndex, o));
            } else if (out.target.type() == typeid(TransactionOutputMultisignature)) {
              MultisignatureOutputUsage usage = {transactionIndex, o, false};
              m_multisignatureOutputs[out.amount].push_back(usage);
            }
          }
        }
      }

      std::chrono::duration<double> duration = std::chrono::steady_clock::now() - timePoint;
      logger(INFO, BRIGHT_WHITE) << "Rebuilding internal structures took: " << duration.count();
    }
  } else {
    m_blocks.clear();
  }

  if (m_blocks.empty()) {
    logger(INFO, BRIGHT_WHITE)
      << "Blockchain not loaded, generating genesis block.";
    block_verification_context bvc =
      boost::value_initialized<block_verification_context>();
    add_new_block(m_currency.genesisBlock(), bvc);
    if (bvc.m_verifivation_failed) {
      logger(ERROR, BRIGHT_RED) << "Failed to add genesis block to blockchain";
      return false;
    }
  } else {
    crypto::hash firstBlockHash = get_block_hash(m_blocks[0].bl);
    if (!(firstBlockHash == m_currency.genesisBlockHash())) {
      logger(ERROR, BRIGHT_RED) << "Failed to init: genesis block mismatch. "
        "Probably you set --testnet flag with data "
        "dir with non-test blockchain or another "
        "network.";
      return false;
    }
  }

  if (!m_upgradeDetector.init()) {
    logger(ERROR, BRIGHT_RED) << "Failed to initialize upgrade detector";
    return false;
  }

  update_next_comulative_size_limit();

  uint64_t timestamp_diff = time(NULL) - m_blocks.back().bl.timestamp;
  if (!m_blocks.back().bl.timestamp) {
    timestamp_diff = time(NULL) - 1341378000;
  }

  logger(INFO, BRIGHT_GREEN)
    << "Blockchain initialized. last block: " << m_blocks.size() - 1 << ", "
    << Common::timeIntervalToString(timestamp_diff)
    << " time ago, current difficulty: " << get_difficulty_for_next_block();
  return true;
}

bool blockchain_storage::storeCache() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  logger(INFO, BRIGHT_WHITE) << "Saving blockchain...";
  BlockCacheSerializer ser(*this, get_tail_id(), logger.getLogger());
  if (!tools::serialize_obj_to_file(ser, appendPath(m_config_folder, m_currency.blocksCacheFileName()))) {
    logger(ERROR, BRIGHT_RED) << "Failed to save blockchain cache";
    return false;
  }

  return true;
}

bool blockchain_storage::deinit() {
  storeCache();
  return true;
}

bool blockchain_storage::reset_and_set_genesis_block(const Block& b) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  m_blocks.clear();
  m_blockIndex.clear();
  m_transactionMap.clear();

  m_spent_keys.clear();
  m_alternative_chains.clear();
  m_outputs.clear();

  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  add_new_block(b, bvc);
  return bvc.m_added_to_main_chain && !bvc.m_verifivation_failed;
}

crypto::hash blockchain_storage::get_tail_id(uint64_t& height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  height = get_current_blockchain_height() - 1;
  return get_tail_id();
}

crypto::hash blockchain_storage::get_tail_id() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_blockIndex.getTailId();
}

bool blockchain_storage::getPoolSymmetricDifference(const std::vector<crypto::hash>& known_pool_tx_ids, const crypto::hash& known_block_id, std::vector<Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids) {
  
  std::lock_guard<decltype(m_tx_pool)> txLock(m_tx_pool);
  std::lock_guard<decltype(m_blockchain_lock)> bcLock(m_blockchain_lock);

  if (known_block_id != get_tail_id()) {
    return false;
  }

  std::vector<crypto::hash> new_tx_ids;
  m_tx_pool.get_difference(known_pool_tx_ids, new_tx_ids, deleted_tx_ids);

  std::vector<crypto::hash> misses;
  get_transactions(new_tx_ids, new_txs, misses, true);
  assert(misses.empty());
  return true;
}

bool blockchain_storage::get_short_chain_history(std::list<crypto::hash>& ids) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_blockIndex.getShortChainHistory(ids);
}

crypto::hash blockchain_storage::get_block_id_by_height(uint64_t height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_blockIndex.getBlockId(height);
}

bool blockchain_storage::get_block_by_hash(const crypto::hash& blockHash, Block& b) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  uint64_t height = 0;

  if (m_blockIndex.getBlockHeight(blockHash, height)) {
    b = m_blocks[height].bl;
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
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> commulative_difficulties;
  size_t offset = m_blocks.size() - std::min(m_blocks.size(), static_cast<uint64_t>(m_currency.difficultyBlocksCount()));
  if (offset == 0) {
    ++offset;
  }

  for (; offset < m_blocks.size(); offset++) {
    timestamps.push_back(m_blocks[offset].bl.timestamp);
    commulative_difficulties.push_back(m_blocks[offset].cumulative_difficulty);
  }

  return m_currency.nextDifficulty(timestamps, commulative_difficulties);
}

uint64_t blockchain_storage::getCoinsInCirculation() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (m_blocks.empty()) {
    return 0;
  } else {
    return m_blocks.back().already_generated_coins;
  }
}

uint8_t blockchain_storage::get_block_major_version_for_height(uint64_t height) const {
  return height > m_upgradeDetector.upgradeHeight() ? m_upgradeDetector.targetVersion() : BLOCK_MAJOR_VERSION_1;
}

bool blockchain_storage::rollback_blockchain_switching(std::list<Block> &original_chain, size_t rollback_height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  // remove failed subchain
  for (size_t i = m_blocks.size() - 1; i >= rollback_height; i--) {
    popBlock(get_block_hash(m_blocks.back().bl));
  }

  // return back original chain
  for(auto &bl : original_chain) {
    block_verification_context bvc =
      boost::value_initialized<block_verification_context>();
    bool r = pushBlock(bl, bvc);
    if (!(r && bvc.m_added_to_main_chain)) {
      logger(ERROR, BRIGHT_RED) << "PANIC!!! failed to add (again) block while "
        "chain switching during the rollback!";
      return false;
    }
  }

  logger(INFO, BRIGHT_WHITE) << "Rollback success.";
  return true;
}

bool blockchain_storage::switch_to_alternative_blockchain(std::list<blocks_ext_by_hash::iterator>& alt_chain, bool discard_disconnected_chain) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  if (!(alt_chain.size())) {
    logger(ERROR, BRIGHT_RED) << "switch_to_alternative_blockchain: empty chain passed";
    return false;
  }

  size_t split_height = alt_chain.front()->second.height;

  if (!(m_blocks.size() > split_height)) {
    logger(ERROR, BRIGHT_RED) << "switch_to_alternative_blockchain: blockchain size is lower than split height";
    return false;
  }

  //disconnecting old chain
  std::list<Block> disconnected_chain;
  for (size_t i = m_blocks.size() - 1; i >= split_height; i--) {
    Block b = m_blocks[i].bl;
    popBlock(get_block_hash(b));
    //if (!(r)) { logger(ERROR, BRIGHT_RED) << "failed to remove block on chain switching"; return false; }
    disconnected_chain.push_front(b);
  }

  //connecting new alternative chain
  for (auto alt_ch_iter = alt_chain.begin(); alt_ch_iter != alt_chain.end(); alt_ch_iter++) {
    auto ch_ent = *alt_ch_iter;
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    bool r = pushBlock(ch_ent->second.bl, bvc);
    if (!r || !bvc.m_added_to_main_chain) {
      logger(INFO, BRIGHT_WHITE) << "Failed to switch to alternative blockchain";
      rollback_blockchain_switching(disconnected_chain, split_height);
      //add_block_as_invalid(ch_ent->second, get_block_hash(ch_ent->second.bl));
      logger(INFO, BRIGHT_WHITE) << "The block was inserted as invalid while connecting new alternative chain,  block_id: " << get_block_hash(ch_ent->second.bl);
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
        logger(ERROR, BRIGHT_RED) << ("Failed to push ex-main chain blocks to alternative chain ");
        rollback_blockchain_switching(disconnected_chain, split_height);
        return false;
      }
    }
  }

  //removing all_chain entries from alternative chain
  for (auto ch_ent : alt_chain) {
    m_alternative_chains.erase(ch_ent);
  }

  logger(INFO, BRIGHT_GREEN) << "REORGANIZE SUCCESS! on height: " << split_height << ", new blockchain size: " << m_blocks.size();
  return true;
}

difficulty_type blockchain_storage::get_next_difficulty_for_alternative_chain(const std::list<blocks_ext_by_hash::iterator>& alt_chain, BlockEntry& bei) {
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> commulative_difficulties;
  if (alt_chain.size() < m_currency.difficultyBlocksCount()) {
    std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
    size_t main_chain_stop_offset = alt_chain.size() ? alt_chain.front()->second.height : bei.height;
    size_t main_chain_count = m_currency.difficultyBlocksCount() - std::min(m_currency.difficultyBlocksCount(), alt_chain.size());
    main_chain_count = std::min(main_chain_count, main_chain_stop_offset);
    size_t main_chain_start_offset = main_chain_stop_offset - main_chain_count;

    if (!main_chain_start_offset)
      ++main_chain_start_offset; //skip genesis block
    for (; main_chain_start_offset < main_chain_stop_offset; ++main_chain_start_offset) {
      timestamps.push_back(m_blocks[main_chain_start_offset].bl.timestamp);
      commulative_difficulties.push_back(m_blocks[main_chain_start_offset].cumulative_difficulty);
    }

    if (!((alt_chain.size() + timestamps.size()) <= m_currency.difficultyBlocksCount())) {
      logger(ERROR, BRIGHT_RED) << "Internal error, alt_chain.size()[" << alt_chain.size() << "] + timestamps.size()[" << timestamps.size() <<
        "] NOT <= m_currency.difficultyBlocksCount()[" << m_currency.difficultyBlocksCount() << ']'; return false;
    }
    for (auto it : alt_chain) {
      timestamps.push_back(it->second.bl.timestamp);
      commulative_difficulties.push_back(it->second.cumulative_difficulty);
    }
  } else {
    timestamps.resize(std::min(alt_chain.size(), m_currency.difficultyBlocksCount()));
    commulative_difficulties.resize(std::min(alt_chain.size(), m_currency.difficultyBlocksCount()));
    size_t count = 0;
    size_t max_i = timestamps.size() - 1;
    BOOST_REVERSE_FOREACH(auto it, alt_chain) {
      timestamps[max_i - count] = it->second.bl.timestamp;
      commulative_difficulties[max_i - count] = it->second.cumulative_difficulty;
      count++;
      if (count >= m_currency.difficultyBlocksCount()) {
        break;
      }
    }
  }

  return m_currency.nextDifficulty(timestamps, commulative_difficulties);
}

bool blockchain_storage::prevalidate_miner_transaction(const Block& b, uint64_t height) {

  if (!(b.minerTx.vin.size() == 1)) {
    logger(ERROR, BRIGHT_RED)
      << "coinbase transaction in the block has no inputs";
    return false;
  }

  if (!(b.minerTx.vin[0].type() == typeid(TransactionInputGenerate))) {
    logger(ERROR, BRIGHT_RED)
      << "coinbase transaction in the block has the wrong type";
    return false;
  }

  if (boost::get<TransactionInputGenerate>(b.minerTx.vin[0]).height != height) {
    logger(INFO, BRIGHT_RED) << "The miner transaction in block has invalid height: " <<
      boost::get<TransactionInputGenerate>(b.minerTx.vin[0]).height << ", expected: " << height;
    return false;
  }

  if (!(b.minerTx.unlockTime == height + m_currency.minedMoneyUnlockWindow())) {
    logger(ERROR, BRIGHT_RED)
      << "coinbase transaction transaction have wrong unlock time="
      << b.minerTx.unlockTime << ", expected "
      << height + m_currency.minedMoneyUnlockWindow();
    return false;
  }

  if (!check_outs_overflow(b.minerTx)) {
    logger(INFO, BRIGHT_RED) << "miner transaction have money overflow in block " << get_block_hash(b);
    return false;
  }

  return true;
}

bool blockchain_storage::validate_miner_transaction(const Block& b, uint64_t height, size_t cumulativeBlockSize,
  uint64_t alreadyGeneratedCoins, uint64_t fee,
  uint64_t& reward, int64_t& emissionChange) {
  uint64_t minerReward = 0;
  for (auto& o : b.minerTx.vout) {
    minerReward += o.amount;
  }

  std::vector<size_t> lastBlocksSizes;
  get_last_n_blocks_sizes(lastBlocksSizes, m_currency.rewardBlocksWindow());
  size_t blocksSizeMedian = medianValue(lastBlocksSizes);

  bool penalizeFee = get_block_major_version_for_height(height) > BLOCK_MAJOR_VERSION_1;
  if (!m_currency.getBlockReward(blocksSizeMedian, cumulativeBlockSize, alreadyGeneratedCoins, fee, penalizeFee, reward, emissionChange)) {
    logger(INFO, BRIGHT_WHITE) << "block size " << cumulativeBlockSize << " is bigger than allowed for this blockchain";
    return false;
  }

  if (minerReward > reward) {
    logger(ERROR, BRIGHT_RED) << "Coinbase transaction spend too much money: " << m_currency.formatAmount(minerReward) <<
      ", block reward is " << m_currency.formatAmount(reward);
    return false;
  } else if (minerReward < reward) {
    logger(ERROR, BRIGHT_RED) << "Coinbase transaction doesn't use full amount of block reward: spent " <<
      m_currency.formatAmount(minerReward) << ", block reward is " << m_currency.formatAmount(reward);
    return false;
  }

  return true;
}

bool blockchain_storage::get_backward_blocks_sizes(size_t from_height, std::vector<size_t>& sz, size_t count) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!(from_height < m_blocks.size())) {
    logger(ERROR, BRIGHT_RED)
      << "Internal error: get_backward_blocks_sizes called with from_height="
      << from_height << ", blockchain height = " << m_blocks.size();
    return false;
  }
  size_t start_offset = (from_height + 1) - std::min((from_height + 1), count);
  for (size_t i = start_offset; i != from_height + 1; i++) {
    sz.push_back(m_blocks[i].block_cumulative_size);
  }

  return true;
}

bool blockchain_storage::get_last_n_blocks_sizes(std::vector<size_t>& sz, size_t count) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!m_blocks.size()) {
    return true;
  }

  return get_backward_blocks_sizes(m_blocks.size() - 1, sz, count);
}

uint64_t blockchain_storage::get_current_comulative_blocksize_limit() {
  return m_current_block_cumul_sz_limit;
}

bool blockchain_storage::create_block_template(Block& b, const AccountPublicAddress& miner_address, difficulty_type& diffic, uint32_t& height, const blobdata& ex_nonce) {
  size_t median_size;
  uint64_t already_generated_coins;

  {
    std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
    height = static_cast<uint32_t>(m_blocks.size());
    diffic = get_difficulty_for_next_block();
    if (!(diffic)) {
      logger(ERROR, BRIGHT_RED) << "difficulty overhead.";
      return false;
    }

    b = boost::value_initialized<Block>();
    b.majorVersion = get_block_major_version_for_height(height);

    if (BLOCK_MAJOR_VERSION_1 == b.majorVersion) {
      b.minorVersion = BLOCK_MINOR_VERSION_1;
    } else if (BLOCK_MAJOR_VERSION_2 == b.majorVersion) {
      b.minorVersion = BLOCK_MINOR_VERSION_0;

      b.parentBlock.majorVersion = BLOCK_MAJOR_VERSION_1;
      b.parentBlock.majorVersion = BLOCK_MINOR_VERSION_0;
      b.parentBlock.numberOfTransactions = 1;
      tx_extra_merge_mining_tag mm_tag = boost::value_initialized<decltype(mm_tag)>();

      if (!append_mm_tag_to_extra(b.parentBlock.minerTx.extra, mm_tag)) {
        logger(ERROR, BRIGHT_RED) << "Failed to append merge mining tag to extra of the parent block miner transaction";
        return false;
      }
    }

    b.prevId = get_tail_id();
    b.timestamp = time(NULL);

    median_size = m_current_block_cumul_sz_limit / 2;
    already_generated_coins = m_blocks.back().already_generated_coins;
  }

  size_t txs_size;
  uint64_t fee;
  if (!m_tx_pool.fill_block_template(b, median_size, m_currency.maxBlockCumulativeSize(height), already_generated_coins,
    txs_size, fee)) {
    return false;
  }

#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
  size_t real_txs_size = 0;
  uint64_t real_fee = 0;
  CRITICAL_REGION_BEGIN(m_tx_pool.m_transactions_lock);
  for (crypto::hash &cur_hash : b.txHashes) {
    auto cur_res = m_tx_pool.m_transactions.find(cur_hash);
    if (cur_res == m_tx_pool.m_transactions.end()) {
      logger(ERROR, BRIGHT_RED) << "Creating block template: error: transaction not found";
      continue;
    }
    tx_memory_pool::tx_details &cur_tx = cur_res->second;
    real_txs_size += cur_tx.blob_size;
    real_fee += cur_tx.fee;
    if (cur_tx.blob_size != get_object_blobsize(cur_tx.tx)) {
      logger(ERROR, BRIGHT_RED) << "Creating block template: error: invalid transaction size";
    }
    uint64_t inputs_amount;
    if (!get_inputs_money_amount(cur_tx.tx, inputs_amount)) {
      logger(ERROR, BRIGHT_RED) << "Creating block template: error: cannot get inputs amount";
    } else if (cur_tx.fee != inputs_amount - get_outs_money_amount(cur_tx.tx)) {
      logger(ERROR, BRIGHT_RED) << "Creating block template: error: invalid fee";
    }
  }
  if (txs_size != real_txs_size) {
    logger(ERROR, BRIGHT_RED) << "Creating block template: error: wrongly calculated transaction size";
  }
  if (fee != real_fee) {
    logger(ERROR, BRIGHT_RED) << "Creating block template: error: wrongly calculated fee";
  }
  CRITICAL_REGION_END();
  logger(DEBUGGING) << "Creating block template: height " << height <<
    ", median size " << median_size <<
    ", already generated coins " << already_generated_coins <<
    ", transaction size " << txs_size <<
    ", fee " << fee;
#endif

  /*
     two-phase miner transaction generation: we don't know exact block size until we prepare block, but we don't know reward until we know
     block size, so first miner transaction generated with fake amount of money, and with phase we know think we know expected block size
     */
  //make blocks coin-base tx looks close to real coinbase tx to get truthful blob size
  bool penalizeFee = b.majorVersion > BLOCK_MAJOR_VERSION_1;
  bool r = m_currency.constructMinerTx(height, median_size, already_generated_coins, txs_size, fee, miner_address, b.minerTx, ex_nonce, 11, penalizeFee);
  if (!r) { 
    logger(ERROR, BRIGHT_RED) << "Failed to construc miner tx, first chance"; 
    return false; 
  }

  size_t cumulative_size = txs_size + get_object_blobsize(b.minerTx);
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
  logger(DEBUGGING) << "Creating block template: miner tx size " << get_object_blobsize(b.minerTx) <<
    ", cumulative size " << cumulative_size;
#endif
  for (size_t try_count = 0; try_count != 10; ++try_count) {
    r = m_currency.constructMinerTx(height, median_size, already_generated_coins, cumulative_size, fee, miner_address, b.minerTx, ex_nonce, 11, penalizeFee);

    if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to construc miner tx, second chance"; return false; }
    size_t coinbase_blob_size = get_object_blobsize(b.minerTx);
    if (coinbase_blob_size > cumulative_size - txs_size) {
      cumulative_size = txs_size + coinbase_blob_size;
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
      logger(DEBUGGING) << "Creating block template: miner tx size " << coinbase_blob_size <<
        ", cumulative size " << cumulative_size << " is greater then before";
#endif
      continue;
    }

    if (coinbase_blob_size < cumulative_size - txs_size) {
      size_t delta = cumulative_size - txs_size - coinbase_blob_size;
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
      logger(DEBUGGING) << "Creating block template: miner tx size " << coinbase_blob_size <<
        ", cumulative size " << txs_size + coinbase_blob_size <<
        " is less then before, adding " << delta << " zero bytes";
#endif
      b.minerTx.extra.insert(b.minerTx.extra.end(), delta, 0);
      //here  could be 1 byte difference, because of extra field counter is varint, and it can become from 1-byte len to 2-bytes len.
      if (cumulative_size != txs_size + get_object_blobsize(b.minerTx)) {
        if (!(cumulative_size + 1 == txs_size + get_object_blobsize(b.minerTx))) { logger(ERROR, BRIGHT_RED) << "unexpected case: cumulative_size=" << cumulative_size << " + 1 is not equal txs_cumulative_size=" << txs_size << " + get_object_blobsize(b.minerTx)=" << get_object_blobsize(b.minerTx); return false; }
        b.minerTx.extra.resize(b.minerTx.extra.size() - 1);
        if (cumulative_size != txs_size + get_object_blobsize(b.minerTx)) {
          //fuck, not lucky, -1 makes varint-counter size smaller, in that case we continue to grow with cumulative_size
          logger(TRACE, BRIGHT_RED) <<
            "Miner tx creation have no luck with delta_extra size = " << delta << " and " << delta - 1;
          cumulative_size += delta - 1;
          continue;
        }
        logger(DEBUGGING, BRIGHT_GREEN) <<
          "Setting extra for block: " << b.minerTx.extra.size() << ", try_count=" << try_count;
      }
    }
    if (!(cumulative_size == txs_size + get_object_blobsize(b.minerTx))) { logger(ERROR, BRIGHT_RED) << "unexpected case: cumulative_size=" << cumulative_size << " is not equal txs_cumulative_size=" << txs_size << " + get_object_blobsize(b.minerTx)=" << get_object_blobsize(b.minerTx); return false; }
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
    logger(DEBUGGING) << "Creating block template: miner tx size " << coinbase_blob_size <<
      ", cumulative size " << cumulative_size << " is now good";
#endif
    return true;
  }

  logger(ERROR, BRIGHT_RED) <<
    "Failed to create_block_template with " << 10 << " tries";
  return false;
}

bool blockchain_storage::complete_timestamps_vector(uint64_t start_top_height, std::vector<uint64_t>& timestamps) {
  if (timestamps.size() >= m_currency.timestampCheckWindow())
    return true;

  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  size_t need_elements = m_currency.timestampCheckWindow() - timestamps.size();
  if (!(start_top_height < m_blocks.size())) { logger(ERROR, BRIGHT_RED) << "internal error: passed start_height = " << start_top_height << " not less then m_blocks.size()=" << m_blocks.size(); return false; }
  size_t stop_offset = start_top_height > need_elements ? start_top_height - need_elements : 0;
  do {
    timestamps.push_back(m_blocks[start_top_height].bl.timestamp);
    if (start_top_height == 0)
      break;
    --start_top_height;
  } while (start_top_height != stop_offset);
  return true;
}

bool blockchain_storage::handle_alternative_block(const Block& b, const crypto::hash& id, block_verification_context& bvc) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  uint64_t block_height = get_block_height(b);
  if (block_height == 0) {
    logger(ERROR, BRIGHT_RED) <<
      "Block with id: " << Common::podToHex(id) << " (as alternative) have wrong miner transaction";
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (!m_checkpoints.is_alternative_block_allowed(get_current_blockchain_height(), block_height)) {
    logger(TRACE) << "Block with id: " << id << std::endl <<
      " can't be accepted for alternative chain, block height: " << block_height << std::endl <<
      " blockchain height: " << get_current_blockchain_height();
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (!checkBlockVersion(b, id)) {
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (!checkParentBlockSize(b, id)) {
    bvc.m_verifivation_failed = true;
    return false;
  }

  size_t cumulativeSize;
  if (!getBlockCumulativeSize(b, cumulativeSize)) {
    logger(TRACE) << "Block with id: " << id << " has at least one unknown transaction. Cumulative size is calculated imprecisely";
  }

  if (!checkCumulativeBlockSize(id, cumulativeSize, block_height)) {
    bvc.m_verifivation_failed = true;
    return false;
  }

  //block is not related with head of main chain
  //first of all - look in alternative chains container
  uint64_t mainPrevHeight = 0;
  const bool mainPrev = m_blockIndex.getBlockHeight(b.prevId, mainPrevHeight);
  const auto it_prev = m_alternative_chains.find(b.prevId);

  if (it_prev != m_alternative_chains.end() || mainPrev) {
    //we have new block in alternative chain

    //build alternative subchain, front -> mainchain, back -> alternative head
    blocks_ext_by_hash::iterator alt_it = it_prev; //m_alternative_chains.find()
    std::list<blocks_ext_by_hash::iterator> alt_chain;
    std::vector<uint64_t> timestamps;
    while (alt_it != m_alternative_chains.end()) {
      alt_chain.push_front(alt_it);
      timestamps.push_back(alt_it->second.bl.timestamp);
      alt_it = m_alternative_chains.find(alt_it->second.bl.prevId);
    }

    if (alt_chain.size()) {
      //make sure that it has right connection to main chain
      if (!(m_blocks.size() > alt_chain.front()->second.height)) { logger(ERROR, BRIGHT_RED) << "main blockchain wrong height"; return false; }
      crypto::hash h = null_hash;
      get_block_hash(m_blocks[alt_chain.front()->second.height - 1].bl, h);
      if (!(h == alt_chain.front()->second.bl.prevId)) { logger(ERROR, BRIGHT_RED) << "alternative chain have wrong connection to main chain"; return false; }
      complete_timestamps_vector(alt_chain.front()->second.height - 1, timestamps);
    } else {
      if (!(mainPrev)) { logger(ERROR, BRIGHT_RED) << "internal error: broken imperative condition it_main_prev != m_blocks_index.end()"; return false; }
      complete_timestamps_vector(mainPrevHeight, timestamps);
    }

    //check timestamp correct
    if (!check_block_timestamp(timestamps, b)) {
      logger(INFO, BRIGHT_RED) <<
        "Block with id: " << id
        << ENDL << " for alternative chain, have invalid timestamp: " << b.timestamp;
      //add_block_as_invalid(b, id);//do not add blocks to invalid storage before proof of work check was passed
      bvc.m_verifivation_failed = true;
      return false;
    }

    BlockEntry bei = boost::value_initialized<BlockEntry>();
    bei.bl = b;
    bei.height = static_cast<uint32_t>(alt_chain.size() ? it_prev->second.height + 1 : mainPrevHeight + 1);

    bool is_a_checkpoint;
    if (!m_checkpoints.check_block(bei.height, id, is_a_checkpoint)) {
      logger(ERROR, BRIGHT_RED) <<
        "CHECKPOINT VALIDATION FAILED";
      bvc.m_verifivation_failed = true;
      return false;
    }

    // Always check PoW for alternative blocks
    m_is_in_checkpoint_zone = false;
    difficulty_type current_diff = get_next_difficulty_for_alternative_chain(alt_chain, bei);
    if (!(current_diff)) { logger(ERROR, BRIGHT_RED) << "!!!!!!! DIFFICULTY OVERHEAD !!!!!!!"; return false; }
    crypto::hash proof_of_work = null_hash;
    if (!m_currency.checkProofOfWork(m_cn_context, bei.bl, current_diff, proof_of_work)) {
      logger(INFO, BRIGHT_RED) <<
        "Block with id: " << id
        << ENDL << " for alternative chain, have not enough proof of work: " << proof_of_work
        << ENDL << " expected difficulty: " << current_diff;
      bvc.m_verifivation_failed = true;
      return false;
    }

    if (!prevalidate_miner_transaction(b, bei.height)) {
      logger(INFO, BRIGHT_RED) <<
        "Block with id: " << Common::podToHex(id) << " (as alternative) have wrong miner transaction.";
      bvc.m_verifivation_failed = true;
      return false;
    }

    bei.cumulative_difficulty = alt_chain.size() ? it_prev->second.cumulative_difficulty : m_blocks[mainPrevHeight].cumulative_difficulty;
    bei.cumulative_difficulty += current_diff;

#ifdef _DEBUG
    auto i_dres = m_alternative_chains.find(id);
    if (!(i_dres == m_alternative_chains.end())) { logger(ERROR, BRIGHT_RED) << "insertion of new alternative block returned as it already exist"; return false; }
#endif

    auto i_res = m_alternative_chains.insert(blocks_ext_by_hash::value_type(id, bei));
    if (!(i_res.second)) { logger(ERROR, BRIGHT_RED) << "insertion of new alternative block returned as it already exist"; return false; }
    alt_chain.push_back(i_res.first);

    if (is_a_checkpoint) {
      //do reorganize!
      logger(INFO, BRIGHT_GREEN) <<
        "###### REORGANIZE on height: " << alt_chain.front()->second.height << " of " << m_blocks.size() - 1 <<
        ", checkpoint is found in alternative chain on height " << bei.height;
      bool r = switch_to_alternative_blockchain(alt_chain, true);
      if (r) bvc.m_added_to_main_chain = true;
      else bvc.m_verifivation_failed = true;
      return r;
    } else if (m_blocks.back().cumulative_difficulty < bei.cumulative_difficulty) //check if difficulty bigger then in main chain
    {
      //do reorganize!
      logger(INFO, BRIGHT_GREEN) <<
        "###### REORGANIZE on height: " << alt_chain.front()->second.height << " of " << m_blocks.size() - 1 << " with cum_difficulty " << m_blocks.back().cumulative_difficulty
        << ENDL << " alternative blockchain size: " << alt_chain.size() << " with cum_difficulty " << bei.cumulative_difficulty;
      bool r = switch_to_alternative_blockchain(alt_chain, false);
      if (r) bvc.m_added_to_main_chain = true;
      else bvc.m_verifivation_failed = true;
      return r;
    } else {
      logger(INFO, BRIGHT_BLUE) <<
        "----- BLOCK ADDED AS ALTERNATIVE ON HEIGHT " << bei.height
        << ENDL << "id:\t" << id
        << ENDL << "PoW:\t" << proof_of_work
        << ENDL << "difficulty:\t" << current_diff;
      return true;
    }
    } else {
    //block orphaned
    bvc.m_marked_as_orphaned = true;
    logger(INFO, BRIGHT_RED) <<
      "Block recognized as orphaned and rejected, id = " << id;
  }

  return true;
    }

bool blockchain_storage::get_blocks(uint64_t start_offset, size_t count, std::list<Block>& blocks, std::list<Transaction>& txs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (start_offset >= m_blocks.size())
    return false;
  for (size_t i = start_offset; i < start_offset + count && i < m_blocks.size(); i++) {
    blocks.push_back(m_blocks[i].bl);
    std::list<crypto::hash> missed_ids;
    get_transactions(m_blocks[i].bl.txHashes, txs, missed_ids);
    if (!(!missed_ids.size())) { logger(ERROR, BRIGHT_RED) << "have missed transactions in own block in main blockchain"; return false; }
  }

  return true;
}

bool blockchain_storage::get_blocks(uint64_t start_offset, size_t count, std::list<Block>& blocks) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (start_offset >= m_blocks.size()) {
    return false;
  }

  for (size_t i = start_offset; i < start_offset + count && i < m_blocks.size(); i++) {
    blocks.push_back(m_blocks[i].bl);
  }

  return true;
}

bool blockchain_storage::handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  rsp.current_blockchain_height = get_current_blockchain_height();
  std::list<Block> blocks;
  get_blocks(arg.blocks, blocks, rsp.missed_ids);

  for (const auto& bl : blocks) {
    std::list<crypto::hash> missed_tx_id;
    std::list<Transaction> txs;
    get_transactions(bl.txHashes, txs, rsp.missed_ids);
    if (!(!missed_tx_id.size())) { logger(ERROR, BRIGHT_RED) << "Internal error: have missed missed_tx_id.size()=" << missed_tx_id.size() << ENDL << "for block id = " << get_block_hash(bl); return false; }
    rsp.blocks.push_back(block_complete_entry());
    block_complete_entry& e = rsp.blocks.back();
    //pack block
    e.block = t_serializable_object_to_blob(bl);
    //pack transactions
    for (Transaction& tx : txs) {
      e.txs.push_back(t_serializable_object_to_blob(tx));
    }
  }

  //get another transactions, if need
  std::list<Transaction> txs;
  get_transactions(arg.txs, txs, rsp.missed_ids);
  //pack aside transactions
  for (const auto& tx : txs) {
    rsp.txs.push_back(t_serializable_object_to_blob(tx));
  }

  return true;
}

bool blockchain_storage::get_alternative_blocks(std::list<Block>& blocks) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  for (auto& alt_bl : m_alternative_chains) {
    blocks.push_back(alt_bl.second.bl);
  }

  return true;
}

size_t blockchain_storage::get_alternative_blocks_count() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_alternative_chains.size();
}

bool blockchain_storage::add_out_to_get_random_outs(std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs, uint64_t amount, size_t i) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  const Transaction& tx = transactionByIndex(amount_outs[i].first).tx;
  if (!(tx.vout.size() > amount_outs[i].second)) {
    logger(ERROR, BRIGHT_RED) << "internal error: in global outs index, transaction out index="
      << amount_outs[i].second << " more than transaction outputs = " << tx.vout.size() << ", for tx id = " << get_transaction_hash(tx); return false;
  }
  if (!(tx.vout[amount_outs[i].second].target.type() == typeid(TransactionOutputToKey))) { logger(ERROR, BRIGHT_RED) << "unknown tx out type"; return false; }

  //check if transaction is unlocked
  if (!is_tx_spendtime_unlocked(tx.unlockTime))
    return false;

  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& oen = *result_outs.outs.insert(result_outs.outs.end(), COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry());
  oen.global_amount_index = i;
  oen.out_key = boost::get<TransactionOutputToKey>(tx.vout[amount_outs[i].second].target).key;
  return true;
}

size_t blockchain_storage::find_end_of_allowed_index(const std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (amount_outs.empty()) {
    return 0;
  }

  size_t i = amount_outs.size();
  do {
    --i;
    if (amount_outs[i].first.block + m_currency.minedMoneyUnlockWindow() <= get_current_blockchain_height()) {
      return i + 1;
    }
  } while (i != 0);

  return 0;
}

bool blockchain_storage::get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  for (uint64_t amount : req.amounts) {
    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs = *res.outs.insert(res.outs.end(), COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount());
    result_outs.amount = amount;
    auto it = m_outputs.find(amount);
    if (it == m_outputs.end()) {
      logger(ERROR, BRIGHT_RED) <<
        "COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS: not outs for amount " << amount << ", wallet should use some real outs when it lookup for some mix, so, at least one out for this amount should exist";
      continue;//actually this is strange situation, wallet should use some real outs when it lookup for some mix, so, at least one out for this amount should exist
    }

    std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs = it->second;
    //it is not good idea to use top fresh outs, because it increases possibility of transaction canceling on split
    //lets find upper bound of not fresh outs
    size_t up_index_limit = find_end_of_allowed_index(amount_outs);
    if (!(up_index_limit <= amount_outs.size())) { logger(ERROR, BRIGHT_RED) << "internal error: find_end_of_allowed_index returned wrong index=" << up_index_limit << ", with amount_outs.size = " << amount_outs.size(); return false; }

    if (up_index_limit > 0) {
      ShuffleGenerator<size_t, crypto::random_engine<size_t>> generator(up_index_limit);
      for (uint64_t j = 0; j < up_index_limit && result_outs.outs.size() < req.outs_count; ++j) {
        add_out_to_get_random_outs(amount_outs, result_outs, amount, generator());
      }
    }
  }
  return true;
}

bool blockchain_storage::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, uint64_t& starter_offset) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  if (!qblock_ids.size() /*|| !req.m_total_height*/) {
    logger(ERROR, BRIGHT_RED) <<
      "Client sent wrong NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << qblock_ids.size() << /*", m_height=" << req.m_total_height <<*/ ", dropping connection";
    return false;
  }
  //check genesis match
  if (qblock_ids.back() != get_block_hash(m_blocks[0].bl)) {
    logger(ERROR, BRIGHT_RED) <<
      "Client sent wrong NOTIFY_REQUEST_CHAIN: genesis block missmatch: " << ENDL << "id: "
      << qblock_ids.back() << ", " << ENDL << "expected: " << get_block_hash(m_blocks[0].bl)
      << "," << ENDL << " dropping connection";
    return false;
  }

  /* Figure out what blocks we should request to get state_normal */
  if (m_blockIndex.findSupplement(qblock_ids, starter_offset)) {
    return true;
  }

  //this should NEVER happen, but, dose of paranoia in such cases is not too bad
  logger(ERROR, BRIGHT_RED) <<
    "Internal error handling connection, can't find split point";
  return false;
}

uint64_t blockchain_storage::block_difficulty(size_t i) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!(i < m_blocks.size())) { logger(ERROR, BRIGHT_RED) << "wrong block index i = " << i << " at blockchain_storage::block_difficulty()"; return false; }
  if (i == 0)
    return m_blocks[i].cumulative_difficulty;

  return m_blocks[i].cumulative_difficulty - m_blocks[i - 1].cumulative_difficulty;
}

void blockchain_storage::print_blockchain(uint64_t start_index, uint64_t end_index) {
  std::stringstream ss;
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (start_index >= m_blocks.size()) {
    logger(INFO, BRIGHT_WHITE) <<
      "Wrong starter index set: " << start_index << ", expected max index " << m_blocks.size() - 1;
    return;
  }

  for (size_t i = start_index; i != m_blocks.size() && i != end_index; i++) {
    ss << "height " << i << ", timestamp " << m_blocks[i].bl.timestamp << ", cumul_dif " << m_blocks[i].cumulative_difficulty << ", cumul_size " << m_blocks[i].block_cumulative_size
      << "\nid\t\t" << get_block_hash(m_blocks[i].bl)
      << "\ndifficulty\t\t" << block_difficulty(i) << ", nonce " << m_blocks[i].bl.nonce << ", tx_count " << m_blocks[i].bl.txHashes.size() << ENDL;
  }
  logger(DEBUGGING) <<
    "Current blockchain:" << ENDL << ss.str();
  logger(INFO, BRIGHT_WHITE) <<
    "Blockchain printed with log level 1";
}

void blockchain_storage::print_blockchain_index() {
  std::stringstream ss;
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  std::list<crypto::hash> blockIds;
  m_blockIndex.getBlockIds(0, std::numeric_limits<size_t>::max(), blockIds);

  logger(INFO, BRIGHT_WHITE) <<
    "Current blockchain index:" << ENDL;

  size_t height = 0;
  for (auto i = blockIds.begin(); i != blockIds.end(); ++i, ++height) {
    logger(INFO, BRIGHT_WHITE) <<
      "id\t\t" << *i << " height" << height;
  }

}

void blockchain_storage::print_blockchain_outs(const std::string& file) {
  std::stringstream ss;
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  for (const outputs_container::value_type& v : m_outputs) {
    const std::vector<std::pair<TransactionIndex, uint16_t>>& vals = v.second;
    if (!vals.empty()) {
      ss << "amount: " << v.first << ENDL;
      for (size_t i = 0; i != vals.size(); i++) {
        ss << "\t" << get_transaction_hash(transactionByIndex(vals[i].first).tx) << ": " << vals[i].second << ENDL;
      }
    }
  }

  if (Common::saveStringToFile(file, ss.str())) {
    logger(INFO, BRIGHT_WHITE) <<
      "Current outputs index writen to file: " << file;
  } else {
    logger(WARNING, BRIGHT_YELLOW) <<
      "Failed to write current outputs index to file: " << file;
  }
}

bool blockchain_storage::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!find_blockchain_supplement(qblock_ids, resp.start_height))
    return false;

  resp.total_height = get_current_blockchain_height();
  size_t count = 0;

  return m_blockIndex.getBlockIds(resp.start_height, BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT, resp.m_block_ids);
}

bool blockchain_storage::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<Block, std::list<Transaction> > >& blocks, uint64_t& total_height, uint64_t& start_height, size_t max_count) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!find_blockchain_supplement(qblock_ids, start_height)) {
    return false;
  }

  total_height = get_current_blockchain_height();
  size_t count = 0;
  for (size_t i = start_height; i != m_blocks.size() && count < max_count; i++, count++) {
    blocks.resize(blocks.size() + 1);
    blocks.back().first = m_blocks[i].bl;
    std::list<crypto::hash> mis;
    get_transactions(m_blocks[i].bl.txHashes, blocks.back().second, mis);
    if (!(!mis.size())) { logger(ERROR, BRIGHT_RED) << "internal error, transaction from block not found"; return false; }
  }

  return true;
}

bool blockchain_storage::have_block(const crypto::hash& id) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (m_blockIndex.hasBlock(id))
    return true;

  if (m_alternative_chains.count(id))
    return true;

  return false;
}

size_t blockchain_storage::get_total_transactions() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_transactionMap.size();
}

bool blockchain_storage::get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  auto it = m_transactionMap.find(tx_id);
  if (it == m_transactionMap.end()) {
    logger(WARNING, YELLOW) << "warning: get_tx_outputs_gindexs failed to find transaction with id = " << tx_id;
    return false;
  }

  const TransactionEntry& tx = transactionByIndex(it->second);
  if (!(tx.m_global_output_indexes.size())) { logger(ERROR, BRIGHT_RED) << "internal error: global indexes for transaction " << tx_id << " is empty"; return false; }
  indexs.resize(tx.m_global_output_indexes.size());
  for (size_t i = 0; i < tx.m_global_output_indexes.size(); ++i) {
    indexs[i] = tx.m_global_output_indexes[i];
  }

  return true;
}

bool blockchain_storage::check_tx_inputs(const Transaction& tx, uint64_t& max_used_block_height, crypto::hash& max_used_block_id, BlockInfo* tail) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  if (tail)
    tail->id = get_tail_id(tail->height);

  bool res = check_tx_inputs(tx, &max_used_block_height);
  if (!res) return false;
  if (!(max_used_block_height < m_blocks.size())) { logger(ERROR, BRIGHT_RED) << "internal error: max used block index=" << max_used_block_height << " is not less then blockchain size = " << m_blocks.size(); return false; }
  get_block_hash(m_blocks[max_used_block_height].bl, max_used_block_id);
  return true;
}

bool blockchain_storage::have_tx_keyimges_as_spent(const Transaction &tx) {
  for (const auto& in : tx.vin) {
    if (in.type() == typeid(TransactionInputToKey)) {
      if (have_tx_keyimg_as_spent(boost::get<TransactionInputToKey>(in).keyImage)) {
        return true;
      }
    }
  }

  return false;
}

bool blockchain_storage::check_tx_inputs(const Transaction& tx, uint64_t* pmax_used_block_height) {
  crypto::hash tx_prefix_hash = get_transaction_prefix_hash(tx);
  return check_tx_inputs(tx, tx_prefix_hash, pmax_used_block_height);
}

bool blockchain_storage::check_tx_inputs(const Transaction& tx, const crypto::hash& tx_prefix_hash, uint64_t* pmax_used_block_height) {
  size_t inputIndex = 0;
  if (pmax_used_block_height) {
    *pmax_used_block_height = 0;
  }

  crypto::hash transactionHash = get_transaction_hash(tx);
  for (const auto& txin : tx.vin) {
    assert(inputIndex < tx.signatures.size());
    if (txin.type() == typeid(TransactionInputToKey)) {
      const TransactionInputToKey& in_to_key = boost::get<TransactionInputToKey>(txin);
      if (!(!in_to_key.keyOffsets.empty())) { logger(ERROR, BRIGHT_RED) << "empty in_to_key.keyOffsets in transaction with id " << get_transaction_hash(tx); return false; }

      if (have_tx_keyimg_as_spent(in_to_key.keyImage)) {
        logger(DEBUGGING) <<
          "Key image already spent in blockchain: " << Common::podToHex(in_to_key.keyImage);
        return false;
      }

      if (!check_tx_input(in_to_key, tx_prefix_hash, tx.signatures[inputIndex], pmax_used_block_height)) {
        logger(INFO, BRIGHT_WHITE) <<
          "Failed to check ring signature for tx " << transactionHash;
        return false;
      }

      ++inputIndex;
    } else if (txin.type() == typeid(TransactionInputMultisignature)) {
      if (!validateInput(::boost::get<TransactionInputMultisignature>(txin), transactionHash, tx_prefix_hash, tx.signatures[inputIndex])) {
        return false;
      }

      ++inputIndex;
    } else {
      logger(INFO, BRIGHT_WHITE) <<
        "Transaction << " << transactionHash << " contains input of unsupported type.";
      return false;
    }
  }

  return true;
}

bool blockchain_storage::is_tx_spendtime_unlocked(uint64_t unlock_time) {
  if (unlock_time < m_currency.maxBlockHeight()) {
    //interpret as block index
    if (get_current_blockchain_height() - 1 + m_currency.lockedTxAllowedDeltaBlocks() >= unlock_time)
      return true;
    else
      return false;
  } else {
    //interpret as time
    uint64_t current_time = static_cast<uint64_t>(time(NULL));
    if (current_time + m_currency.lockedTxAllowedDeltaSeconds() >= unlock_time)
      return true;
    else
      return false;
  }

  return false;
}

bool blockchain_storage::check_tx_input(const TransactionInputToKey& txin, const crypto::hash& tx_prefix_hash, const std::vector<crypto::signature>& sig, uint64_t* pmax_related_block_height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  struct outputs_visitor {
    std::vector<const crypto::public_key *>& m_results_collector;
    blockchain_storage& m_bch;
    LoggerRef logger;
    outputs_visitor(std::vector<const crypto::public_key *>& results_collector, blockchain_storage& bch, ILogger& logger) :m_results_collector(results_collector), m_bch(bch), logger(logger, "outputs_visitor") {
    }

    bool handle_output(const Transaction& tx, const TransactionOutput& out) {
      //check tx unlock time
      if (!m_bch.is_tx_spendtime_unlocked(tx.unlockTime)) {
        logger(INFO, BRIGHT_WHITE) <<
          "One of outputs for one of inputs have wrong tx.unlockTime = " << tx.unlockTime;
        return false;
      }

      if (out.target.type() != typeid(TransactionOutputToKey)) {
        logger(INFO, BRIGHT_WHITE) <<
          "Output have wrong type id, which=" << out.target.which();
        return false;
      }

      m_results_collector.push_back(&boost::get<TransactionOutputToKey>(out.target).key);
      return true;
    }
  };

  //check ring signature
  std::vector<const crypto::public_key *> output_keys;
  outputs_visitor vi(output_keys, *this, logger.getLogger());
  if (!scan_outputkeys_for_indexes(txin, vi, pmax_related_block_height)) {
    logger(INFO, BRIGHT_WHITE) <<
      "Failed to get output keys for tx with amount = " << m_currency.formatAmount(txin.amount) <<
      " and count indexes " << txin.keyOffsets.size();
    return false;
  }

  if (txin.keyOffsets.size() != output_keys.size()) {
    logger(INFO, BRIGHT_WHITE) <<
      "Output keys for tx with amount = " << txin.amount << " and count indexes " << txin.keyOffsets.size() << " returned wrong keys count " << output_keys.size();
    return false;
  }

  if (!(sig.size() == output_keys.size())) { logger(ERROR, BRIGHT_RED) << "internal error: tx signatures count=" << sig.size() << " mismatch with outputs keys count for inputs=" << output_keys.size(); return false; }
  if (m_is_in_checkpoint_zone) {
    return true;
  }

  return crypto::check_ring_signature(tx_prefix_hash, txin.keyImage, output_keys, sig.data());
}

uint64_t blockchain_storage::get_adjusted_time() {
  //TODO: add collecting median time
  return time(NULL);
}

bool blockchain_storage::check_block_timestamp_main(const Block& b) {
  if (b.timestamp > get_adjusted_time() + m_currency.blockFutureTimeLimit()) {
    logger(INFO, BRIGHT_WHITE) <<
      "Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp << ", bigger than adjusted time + 2 hours";
    return false;
  }

  std::vector<uint64_t> timestamps;
  size_t offset = m_blocks.size() <= m_currency.timestampCheckWindow() ? 0 : m_blocks.size() - m_currency.timestampCheckWindow();
  for (; offset != m_blocks.size(); ++offset) {
    timestamps.push_back(m_blocks[offset].bl.timestamp);
  }

  return check_block_timestamp(std::move(timestamps), b);
}

bool blockchain_storage::check_block_timestamp(std::vector<uint64_t> timestamps, const Block& b) {
  if (timestamps.size() < m_currency.timestampCheckWindow()) {
    return true;
  }

  uint64_t median_ts = medianValue(timestamps);

  if (b.timestamp < median_ts) {
    logger(INFO, BRIGHT_WHITE) <<
      "Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp <<
      ", less than median of last " << m_currency.timestampCheckWindow() << " blocks, " << median_ts;
    return false;
  }

  return true;
}

bool blockchain_storage::checkBlockVersion(const Block& b, const crypto::hash& blockHash) {
  uint64_t height = get_block_height(b);
  const uint8_t expectedBlockVersion = get_block_major_version_for_height(height);
  if (b.majorVersion != expectedBlockVersion) {
    logger(TRACE) << "Block " << blockHash << " has wrong major version: " << static_cast<int>(b.majorVersion) <<
      ", at height " << height << " expected version is " << static_cast<int>(expectedBlockVersion);
    return false;
  }

  return true;
}

bool blockchain_storage::checkParentBlockSize(const Block& b, const crypto::hash& blockHash) {
  if (BLOCK_MAJOR_VERSION_2 == b.majorVersion) {
    auto serializer = makeParentBlockSerializer(b, false, false);
    size_t parentBlockSize;
    if (!get_object_blobsize(serializer, parentBlockSize)) {
      logger(ERROR, BRIGHT_RED) <<
        "Block " << blockHash << ": failed to determine parent block size";
      return false;
    }

    if (parentBlockSize > 2 * 1024) {
      logger(INFO, BRIGHT_WHITE) <<
        "Block " << blockHash << " contains too big parent block: " << parentBlockSize <<
        " bytes, expected no more than " << 2 * 1024 << " bytes";
      return false;
    }
  }

  return true;
}

bool blockchain_storage::checkCumulativeBlockSize(const crypto::hash& blockId, size_t cumulativeBlockSize, uint64_t height) {
  size_t maxBlockCumulativeSize = m_currency.maxBlockCumulativeSize(height);
  if (cumulativeBlockSize > maxBlockCumulativeSize) {
    logger(INFO, BRIGHT_WHITE) <<
      "Block " << blockId << " is too big: " << cumulativeBlockSize << " bytes, " <<
      "exptected no more than " << maxBlockCumulativeSize << " bytes";
    return false;
  }

  return true;
}

// Returns true, if cumulativeSize is calculated precisely, else returns false.
bool blockchain_storage::getBlockCumulativeSize(const Block& block, size_t& cumulativeSize) {
  std::vector<Transaction> blockTxs;
  std::vector<crypto::hash> missedTxs;
  get_transactions(block.txHashes, blockTxs, missedTxs, true);

  cumulativeSize = get_object_blobsize(block.minerTx);
  for (const Transaction& tx : blockTxs) {
    cumulativeSize += get_object_blobsize(tx);
  }

  return missedTxs.empty();
}

// Precondition: m_blockchain_lock is locked.
bool blockchain_storage::update_next_comulative_size_limit() {
  uint8_t nextBlockMajorVersion = get_block_major_version_for_height(m_blocks.size());
  size_t nextBlockGrantedFullRewardZone = nextBlockMajorVersion == BLOCK_MAJOR_VERSION_1 ?
    parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1 :
    m_currency.blockGrantedFullRewardZone();

  std::vector<size_t> sz;
  get_last_n_blocks_sizes(sz, m_currency.rewardBlocksWindow());

  uint64_t median = medianValue(sz);
  if (median <= nextBlockGrantedFullRewardZone) {
    median = nextBlockGrantedFullRewardZone;
  }

  m_current_block_cumul_sz_limit = median * 2;
  return true;
}

bool blockchain_storage::add_new_block(const Block& bl_, block_verification_context& bvc) {
  //copy block here to let modify block.target
  Block bl = bl_;
  crypto::hash id;
  if (!get_block_hash(bl, id)) {
    logger(ERROR, BRIGHT_RED) <<
      "Failed to get block hash, possible block has invalid format";
    bvc.m_verifivation_failed = true;
    return false;
  }

  bool add_result;

  { //to avoid deadlock lets lock tx_pool for whole add/reorganize process
    std::lock_guard<decltype(m_tx_pool)> poolLock(m_tx_pool);
    std::lock_guard<decltype(m_blockchain_lock)> bcLock(m_blockchain_lock);

    if (have_block(id)) {
      logger(TRACE) << "block with id = " << id << " already exists";
      bvc.m_already_exists = true;
      return false;
    }

    //check that block refers to chain tail
    if (!(bl.prevId == get_tail_id())) {
      //chain switching or wrong block
      bvc.m_added_to_main_chain = false;
      add_result = handle_alternative_block(bl, id, bvc);
    } else {
      add_result = pushBlock(bl, bvc);
    }
  }

  if (add_result && bvc.m_added_to_main_chain) {
    m_observerManager.notify(&IBlockchainStorageObserver::blockchainUpdated);
  }

  return add_result;
}

const blockchain_storage::TransactionEntry& blockchain_storage::transactionByIndex(TransactionIndex index) {
  return m_blocks[index.block].transactions[index.transaction];
}

bool blockchain_storage::pushBlock(const Block& blockData, block_verification_context& bvc) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  auto blockProcessingStart = std::chrono::steady_clock::now();

  crypto::hash blockHash = get_block_hash(blockData);

  if (m_blockIndex.hasBlock(blockHash)) {
    logger(ERROR, BRIGHT_RED) <<
      "Block " << blockHash << " already exists in blockchain.";
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (!checkBlockVersion(blockData, blockHash)) {
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (!checkParentBlockSize(blockData, blockHash)) {
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (blockData.prevId != get_tail_id()) {
    logger(INFO, BRIGHT_WHITE) <<
      "Block " << blockHash << " has wrong prevId: " << blockData.prevId << ", expected: " << get_tail_id();
    bvc.m_verifivation_failed = true;
    return false;
  }

  if (!check_block_timestamp_main(blockData)) {
    logger(INFO, BRIGHT_WHITE) <<
      "Block " << blockHash << " has invalid timestamp: " << blockData.timestamp;
    bvc.m_verifivation_failed = true;
    return false;
  }

  auto targetTimeStart = std::chrono::steady_clock::now();
  difficulty_type currentDifficulty = get_difficulty_for_next_block();
  auto target_calculating_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - targetTimeStart).count();

  if (!(currentDifficulty)) { 
    logger(ERROR, BRIGHT_RED) << "!!!!!!!!! difficulty overhead !!!!!!!!!"; 
    return false; 
  }

  
  auto longhashTimeStart = std::chrono::steady_clock::now();
  crypto::hash proof_of_work = null_hash;
  if (m_checkpoints.is_in_checkpoint_zone(get_current_blockchain_height())) {
    if (!m_checkpoints.check_block(get_current_blockchain_height(), blockHash)) {
      logger(ERROR, BRIGHT_RED) <<
        "CHECKPOINT VALIDATION FAILED";
      bvc.m_verifivation_failed = true;
      return false;
    }
  } else {
    if (!m_currency.checkProofOfWork(m_cn_context, blockData, currentDifficulty, proof_of_work)) {
      logger(INFO, BRIGHT_WHITE) <<
        "Block " << blockHash << ", has too weak proof of work: " << proof_of_work << ", expected difficulty: " << currentDifficulty;
      bvc.m_verifivation_failed = true;
      return false;
    }
  }

  auto longhash_calculating_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - longhashTimeStart).count();

  if (!prevalidate_miner_transaction(blockData, m_blocks.size())) {
    logger(INFO, BRIGHT_WHITE) <<
      "Block " << blockHash << " failed to pass prevalidation";
    bvc.m_verifivation_failed = true;
    return false;
  }

  crypto::hash minerTransactionHash = get_transaction_hash(blockData.minerTx);

  BlockEntry block;
  block.bl = blockData;
  block.transactions.resize(1);
  block.transactions[0].tx = blockData.minerTx;
  TransactionIndex transactionIndex = {static_cast<uint32_t>(m_blocks.size()), static_cast<uint16_t>(0)};
  pushTransaction(block, minerTransactionHash, transactionIndex);

  size_t coinbase_blob_size = get_object_blobsize(blockData.minerTx);
  size_t cumulative_block_size = coinbase_blob_size;
  uint64_t fee_summary = 0;
  for (const crypto::hash& tx_id : blockData.txHashes) {
    block.transactions.resize(block.transactions.size() + 1);
    size_t blob_size = 0;
    uint64_t fee = 0;
    if (!m_tx_pool.take_tx(tx_id, block.transactions.back().tx, blob_size, fee)) {
      logger(INFO, BRIGHT_WHITE) <<
        "Block " << blockHash << " has at least one unknown transaction: " << tx_id;
      bvc.m_verifivation_failed = true;
      tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();
      block.transactions.pop_back();
      popTransactions(block, minerTransactionHash);
      return false;
    }

    if (!check_tx_inputs(block.transactions.back().tx)) {
      logger(INFO, BRIGHT_WHITE) <<
        "Block " << blockHash << " has at least one transaction with wrong inputs: " << tx_id;
      bvc.m_verifivation_failed = true;
      tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();;
      if (!m_tx_pool.add_tx(block.transactions.back().tx, tvc, true)) {
        logger(ERROR, BRIGHT_RED) <<
          "Cannot move transaction from blockchain to transaction pool.";
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

  if (!checkCumulativeBlockSize(blockHash, cumulative_block_size, m_blocks.size())) {
    bvc.m_verifivation_failed = true;
    return false;
  }

  int64_t emissionChange = 0;
  uint64_t reward = 0;
  uint64_t already_generated_coins = m_blocks.empty() ? 0 : m_blocks.back().already_generated_coins;
  if (!validate_miner_transaction(blockData, m_blocks.size(), cumulative_block_size, already_generated_coins, fee_summary, reward, emissionChange)) {
    logger(INFO, BRIGHT_WHITE) << "Block " << blockHash << " has invalid miner transaction";
    bvc.m_verifivation_failed = true;
    popTransactions(block, minerTransactionHash);
    return false;
  }

  block.height = static_cast<uint32_t>(m_blocks.size());
  block.block_cumulative_size = cumulative_block_size;
  block.cumulative_difficulty = currentDifficulty;
  block.already_generated_coins = already_generated_coins + emissionChange;
  if (m_blocks.size() > 0) {
    block.cumulative_difficulty += m_blocks.back().cumulative_difficulty;
  }

  pushBlock(block);

  auto block_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - blockProcessingStart).count();

  logger(DEBUGGING) <<
    "+++++ BLOCK SUCCESSFULLY ADDED" << ENDL << "id:\t" << blockHash
    << ENDL << "PoW:\t" << proof_of_work
    << ENDL << "HEIGHT " << block.height << ", difficulty:\t" << currentDifficulty
    << ENDL << "block reward: " << m_currency.formatAmount(reward) << ", fee = " << m_currency.formatAmount(fee_summary)
    << ", coinbase_blob_size: " << coinbase_blob_size << ", cumulative size: " << cumulative_block_size
    << ", " << block_processing_time << "(" << target_calculating_time << "/" << longhash_calculating_time << ")ms";

  bvc.m_added_to_main_chain = true;

  m_upgradeDetector.blockPushed();
  update_next_comulative_size_limit();

  return true;
}

bool blockchain_storage::pushBlock(BlockEntry& block) {
  crypto::hash blockHash = get_block_hash(block.bl);

  m_blocks.push_back(block);
  m_blockIndex.push(blockHash);

  assert(m_blockIndex.size() == m_blocks.size());

  return true;
}

void blockchain_storage::popBlock(const crypto::hash& blockHash) {
  if (m_blocks.empty()) {
    logger(ERROR, BRIGHT_RED) <<
      "Attempt to pop block from empty blockchain.";
    return;
  }

  popTransactions(m_blocks.back(), get_transaction_hash(m_blocks.back().bl.minerTx));
  m_blocks.pop_back();
  m_blockIndex.pop();

  assert(m_blockIndex.size() == m_blocks.size());

  m_upgradeDetector.blockPopped();
}

bool blockchain_storage::pushTransaction(BlockEntry& block, const crypto::hash& transactionHash, TransactionIndex transactionIndex) {
  auto result = m_transactionMap.insert(std::make_pair(transactionHash, transactionIndex));
  if (!result.second) {
    logger(ERROR, BRIGHT_RED) <<
      "Duplicate transaction was pushed to blockchain.";
    return false;
  }

  TransactionEntry& transaction = block.transactions[transactionIndex.transaction];

  if (!checkMultisignatureInputsDiff(transaction.tx)) {
    logger(ERROR, BRIGHT_RED) <<
      "Double spending transaction was pushed to blockchain.";
    m_transactionMap.erase(transactionHash);
    return false;
  }

  for (size_t i = 0; i < transaction.tx.vin.size(); ++i) {
    if (transaction.tx.vin[i].type() == typeid(TransactionInputToKey)) {
      auto result = m_spent_keys.insert(::boost::get<TransactionInputToKey>(transaction.tx.vin[i]).keyImage);
      if (!result.second) {
        logger(ERROR, BRIGHT_RED) <<
          "Double spending transaction was pushed to blockchain.";
        for (size_t j = 0; j < i; ++j) {
          m_spent_keys.erase(::boost::get<TransactionInputToKey>(transaction.tx.vin[i - 1 - j]).keyImage);
        }

        m_transactionMap.erase(transactionHash);
        return false;
      }
    }
  }

  for (const auto& inv : transaction.tx.vin) {
    if (inv.type() == typeid(TransactionInputMultisignature)) {
      const TransactionInputMultisignature& in = ::boost::get<TransactionInputMultisignature>(inv);
      auto& amountOutputs = m_multisignatureOutputs[in.amount];
      amountOutputs[in.outputIndex].isUsed = true;
    }
  }

  transaction.m_global_output_indexes.resize(transaction.tx.vout.size());
  for (uint16_t output = 0; output < transaction.tx.vout.size(); ++output) {
    if (transaction.tx.vout[output].target.type() == typeid(TransactionOutputToKey)) {
      auto& amountOutputs = m_outputs[transaction.tx.vout[output].amount];
      transaction.m_global_output_indexes[output] = static_cast<uint32_t>(amountOutputs.size());
      amountOutputs.push_back(std::make_pair<>(transactionIndex, output));
    } else if (transaction.tx.vout[output].target.type() == typeid(TransactionOutputMultisignature)) {
      auto& amountOutputs = m_multisignatureOutputs[transaction.tx.vout[output].amount];
      transaction.m_global_output_indexes[output] = static_cast<uint32_t>(amountOutputs.size());
      MultisignatureOutputUsage outputUsage = {transactionIndex, output, false};
      amountOutputs.push_back(outputUsage);
    }
  }

  return true;
}

void blockchain_storage::popTransaction(const Transaction& transaction, const crypto::hash& transactionHash) {
  TransactionIndex transactionIndex = m_transactionMap.at(transactionHash);
  for (size_t outputIndex = 0; outputIndex < transaction.vout.size(); ++outputIndex) {
    const TransactionOutput& output = transaction.vout[transaction.vout.size() - 1 - outputIndex];
    if (output.target.type() == typeid(TransactionOutputToKey)) {
      auto amountOutputs = m_outputs.find(output.amount);
      if (amountOutputs == m_outputs.end()) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - cannot find specific amount in outputs map.";
        continue;
      }

      if (amountOutputs->second.empty()) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - output array for specific amount is empty.";
        continue;
      }

      if (amountOutputs->second.back().first.block != transactionIndex.block || amountOutputs->second.back().first.transaction != transactionIndex.transaction) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - invalid transaction index.";
        continue;
      }

      if (amountOutputs->second.back().second != transaction.vout.size() - 1 - outputIndex) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - invalid output index.";
        continue;
      }

      amountOutputs->second.pop_back();
      if (amountOutputs->second.empty()) {
        m_outputs.erase(amountOutputs);
      }
    } else if (output.target.type() == typeid(TransactionOutputMultisignature)) {
      auto amountOutputs = m_multisignatureOutputs.find(output.amount);
      if (amountOutputs == m_multisignatureOutputs.end()) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - cannot find specific amount in outputs map.";
        continue;
      }

      if (amountOutputs->second.empty()) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - output array for specific amount is empty.";
        continue;
      }

      if (amountOutputs->second.back().isUsed) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - attempting to remove used output.";
        continue;
      }

      if (amountOutputs->second.back().transactionIndex.block != transactionIndex.block || amountOutputs->second.back().transactionIndex.transaction != transactionIndex.transaction) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - invalid transaction index.";
        continue;
      }

      if (amountOutputs->second.back().outputIndex != transaction.vout.size() - 1 - outputIndex) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - invalid output index.";
        continue;
      }

      amountOutputs->second.pop_back();
      if (amountOutputs->second.empty()) {
        m_multisignatureOutputs.erase(amountOutputs);
      }
    }
  }

  for (auto& input : transaction.vin) {
    if (input.type() == typeid(TransactionInputToKey)) {
      size_t count = m_spent_keys.erase(::boost::get<TransactionInputToKey>(input).keyImage);
      if (count != 1) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - cannot find spent key.";
      }
    } else if (input.type() == typeid(TransactionInputMultisignature)) {
      const TransactionInputMultisignature& in = ::boost::get<TransactionInputMultisignature>(input);
      auto& amountOutputs = m_multisignatureOutputs[in.amount];
      if (!amountOutputs[in.outputIndex].isUsed) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - multisignature output not marked as used.";
      }

      amountOutputs[in.outputIndex].isUsed = false;
    }
  }

  size_t count = m_transactionMap.erase(transactionHash);
  if (count != 1) {
    logger(ERROR, BRIGHT_RED) <<
      "Blockchain consistency broken - cannot find transaction by hash.";
  }
}

void blockchain_storage::popTransactions(const BlockEntry& block, const crypto::hash& minerTransactionHash) {
  for (size_t i = 0; i < block.transactions.size() - 1; ++i) {
    popTransaction(block.transactions[block.transactions.size() - 1 - i].tx, block.bl.txHashes[block.transactions.size() - 2 - i]);
    tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();
    if (!m_tx_pool.add_tx(block.transactions[block.transactions.size() - 1 - i].tx, tvc, true)) {
      logger(ERROR, BRIGHT_RED) <<
        "Cannot move transaction from blockchain to transaction pool.";
    }
  }

  popTransaction(block.bl.minerTx, minerTransactionHash);
}

bool blockchain_storage::validateInput(const TransactionInputMultisignature& input, const crypto::hash& transactionHash, const crypto::hash& transactionPrefixHash, const std::vector<crypto::signature>& transactionSignatures) {
  assert(input.signatures == transactionSignatures.size());
  MultisignatureOutputsContainer::const_iterator amountOutputs = m_multisignatureOutputs.find(input.amount);
  if (amountOutputs == m_multisignatureOutputs.end()) {
    logger(DEBUGGING) <<
      "Transaction << " << transactionHash << " contains multisignature input with invalid amount.";
    return false;
  }

  if (input.outputIndex >= amountOutputs->second.size()) {
    logger(DEBUGGING) <<
      "Transaction << " << transactionHash << " contains multisignature input with invalid outputIndex.";
    return false;
  }

  const MultisignatureOutputUsage& outputIndex = amountOutputs->second[input.outputIndex];
  if (outputIndex.isUsed) {
    logger(DEBUGGING) <<
      "Transaction << " << transactionHash << " contains double spending multisignature input.";
    return false;
  }

  const Transaction& outputTransaction = m_blocks[outputIndex.transactionIndex.block].transactions[outputIndex.transactionIndex.transaction].tx;
  if (!is_tx_spendtime_unlocked(outputTransaction.unlockTime)) {
    logger(DEBUGGING) <<
      "Transaction << " << transactionHash << " contains multisignature input which points to a locked transaction.";
    return false;
  }

  assert(outputTransaction.vout[outputIndex.outputIndex].amount == input.amount);
  assert(outputTransaction.vout[outputIndex.outputIndex].target.type() == typeid(TransactionOutputMultisignature));
  const TransactionOutputMultisignature& output = ::boost::get<TransactionOutputMultisignature>(outputTransaction.vout[outputIndex.outputIndex].target);
  if (input.signatures != output.requiredSignatures) {
    logger(DEBUGGING) <<
      "Transaction << " << transactionHash << " contains multisignature input with invalid signature count.";
    return false;
  }

  std::size_t inputSignatureIndex = 0;
  std::size_t outputKeyIndex = 0;
  while (inputSignatureIndex < input.signatures) {
    if (outputKeyIndex == output.keys.size()) {
      logger(DEBUGGING) <<
        "Transaction << " << transactionHash << " contains multisignature input with invalid signatures.";
      return false;
    }

    if (crypto::check_signature(transactionPrefixHash, output.keys[outputKeyIndex], transactionSignatures[inputSignatureIndex])) {
      ++inputSignatureIndex;
    }

    ++outputKeyIndex;
  }

  return true;
}

bool blockchain_storage::getLowerBound(uint64_t timestamp, uint64_t startOffset, uint64_t& height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  if (startOffset >= m_blocks.size()) {
    return false;
  }

  auto bound = std::lower_bound(m_blocks.begin() + startOffset, m_blocks.end(), timestamp - m_currency.blockFutureTimeLimit(),
    [](const BlockEntry& b, uint64_t timestamp) { return b.bl.timestamp < timestamp; });

  if (bound == m_blocks.end()) {
    return false;
  }

  height = std::distance(m_blocks.begin(), bound);
  return true;
}

bool blockchain_storage::getBlockIds(uint64_t startHeight, size_t maxCount, std::list<crypto::hash>& items) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_blockIndex.getBlockIds(startHeight, maxCount, items);
}

}
