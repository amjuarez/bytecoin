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

#include "cryptonote_core.h"
#include <sstream>
#include <unordered_set>
#include "../cryptonote_config.h"
#include "../Common/command_line.h"
#include "../Common/util.h"
#include "../crypto/crypto.h"
#include "../cryptonote_protocol/cryptonote_protocol_defs.h"
#include "../Logging/LoggerRef.h"
#include "../rpc/core_rpc_server_commands_defs.h"
#include "cryptonote_format_utils.h"
#include "cryptonote_stat_info.h"
#include "miner.h"
#undef ERROR

using namespace Logging;
#include "cryptonote_core/CoreConfig.h"

namespace CryptoNote {

core::core(const Currency& currency, i_cryptonote_protocol* pprotocol, Logging::ILogger& logger) :
m_currency(currency),
logger(logger, "core"),
m_mempool(currency, m_blockchain_storage, m_timeProvider, logger),
m_blockchain_storage(currency, m_mempool, logger),
m_miner(new miner(currency, *this, logger)),
m_starter_message_showed(false) {
  set_cryptonote_protocol(pprotocol);
  m_blockchain_storage.addObserver(this);
    m_mempool.addObserver(this);
  }
  //-----------------------------------------------------------------------------------------------
  core::~core() {
  m_blockchain_storage.removeObserver(this);
}

void core::set_cryptonote_protocol(i_cryptonote_protocol* pprotocol) {
  if (pprotocol)
    m_pprotocol = pprotocol;
  else
    m_pprotocol = &m_protocol_stub;
}
//-----------------------------------------------------------------------------------
void core::set_checkpoints(checkpoints&& chk_pts) {
  m_blockchain_storage.set_checkpoints(std::move(chk_pts));
}
//-----------------------------------------------------------------------------------
void core::init_options(boost::program_options::options_description& /*desc*/) {
}

bool core::handle_command_line(const boost::program_options::variables_map& vm) {
  m_config_folder = command_line::get_arg(vm, command_line::arg_data_dir);
  return true;
}

bool core::is_ready() {
  return !m_blockchain_storage.is_storing_blockchain();
}


uint64_t core::get_current_blockchain_height() {
  return m_blockchain_storage.get_current_blockchain_height();
}

bool core::get_blockchain_top(uint64_t& height, crypto::hash& top_id) {
  top_id = m_blockchain_storage.get_tail_id(height);
  return true;
}

bool core::get_blocks(uint64_t start_offset, size_t count, std::list<Block>& blocks, std::list<Transaction>& txs) {
  return m_blockchain_storage.get_blocks(start_offset, count, blocks, txs);
}

bool core::get_blocks(uint64_t start_offset, size_t count, std::list<Block>& blocks) {
  return m_blockchain_storage.get_blocks(start_offset, count, blocks);
}  
void core::getTransactions(const std::vector<crypto::hash>& txs_ids, std::list<Transaction>& txs, std::list<crypto::hash>& missed_txs, bool checkTxPool) {
  m_blockchain_storage.get_transactions(txs_ids, txs, missed_txs, checkTxPool);
}

bool core::get_alternative_blocks(std::list<Block>& blocks) {
  return m_blockchain_storage.get_alternative_blocks(blocks);
}

size_t core::get_alternative_blocks_count() {
  return m_blockchain_storage.get_alternative_blocks_count();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::init(const CoreConfig& config, const MinerConfig& minerConfig, bool load_existing) {
    m_config_folder = config.configFolder;
    bool r = m_mempool.init(m_config_folder);
  if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to initialize memory pool"; return false; }

  r = m_blockchain_storage.init(m_config_folder, load_existing);
  if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to initialize blockchain storage"; return false; }

    r = m_miner->init(minerConfig);
  if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to initialize blockchain storage"; return false; }

  return load_state_data();
}

bool core::set_genesis_block(const Block& b) {
  return m_blockchain_storage.reset_and_set_genesis_block(b);
}

bool core::load_state_data() {
  // may be some code later
  return true;
}

bool core::deinit() {
  m_miner->stop();
  m_mempool.deinit();
  m_blockchain_storage.deinit();
  return true;
}

bool core::handle_incoming_tx(const blobdata& tx_blob, tx_verification_context& tvc, bool keeped_by_block) {
  tvc = boost::value_initialized<tx_verification_context>();
  //want to process all transactions sequentially
  std::lock_guard<std::mutex> lk(m_incoming_tx_lock);

  if (tx_blob.size() > m_currency.maxTxSize()) {
    logger(INFO) << "WRONG TRANSACTION BLOB, too big size " << tx_blob.size() << ", rejected";
    tvc.m_verifivation_failed = true;
    return false;
  }

  crypto::hash tx_hash = null_hash;
  crypto::hash tx_prefixt_hash = null_hash;
  Transaction tx;

  if (!parse_tx_from_blob(tx, tx_hash, tx_prefixt_hash, tx_blob)) {
    logger(INFO) << "WRONG TRANSACTION BLOB, Failed to parse, rejected";
    tvc.m_verifivation_failed = true;
    return false;
  }
  //std::cout << "!"<< tx.vin.size() << std::endl;

  if (!check_tx_syntax(tx)) {
    logger(INFO) << "WRONG TRANSACTION BLOB, Failed to check tx " << tx_hash << " syntax, rejected";
    tvc.m_verifivation_failed = true;
    return false;
  }

  if (!check_tx_semantic(tx, keeped_by_block)) {
    logger(INFO) << "WRONG TRANSACTION BLOB, Failed to check tx " << tx_hash << " semantic, rejected";
    tvc.m_verifivation_failed = true;
    return false;
  }

  bool r = add_new_tx(tx, tx_hash, tx_prefixt_hash, tx_blob.size(), tvc, keeped_by_block);
  if (tvc.m_verifivation_failed) {
    if (!tvc.m_tx_fee_too_small) {
      logger(ERROR) << "Transaction verification failed: " << tx_hash;
    } else {
      logger(INFO) << "Transaction verification failed: " << tx_hash;
    }
  } else if (tvc.m_verifivation_impossible) {
    logger(ERROR) << "Transaction verification impossible: " << tx_hash;
  }

  if (tvc.m_added_to_pool) {
    logger(DEBUGGING) << "tx added: " << tx_hash;
      poolUpdated();
  }

  return r;
}

bool core::get_stat_info(core_stat_info& st_inf) {
  st_inf.mining_speed = m_miner->get_speed();
  st_inf.alternative_blocks = m_blockchain_storage.get_alternative_blocks_count();
  st_inf.blockchain_height = m_blockchain_storage.get_current_blockchain_height();
  st_inf.tx_pool_size = m_mempool.get_transactions_count();
  st_inf.top_block_id_str = Common::podToHex(m_blockchain_storage.get_tail_id());
  return true;
}


bool core::check_tx_semantic(const Transaction& tx, bool keeped_by_block) {
  if (!tx.vin.size()) {
    logger(ERROR) << "tx with empty inputs, rejected for tx id= " << get_transaction_hash(tx);
    return false;
  }

  if (!check_inputs_types_supported(tx)) {
    logger(ERROR) << "unsupported input types for tx id= " << get_transaction_hash(tx);
    return false;
  }

  std::string errmsg;
  if (!check_outs_valid(tx, &errmsg)) {
    logger(ERROR) << "tx with invalid outputs, rejected for tx id= " << get_transaction_hash(tx) << ": " << errmsg;
    return false;
  }

  if (!check_money_overflow(tx)) {
    logger(ERROR) << "tx have money overflow, rejected for tx id= " << get_transaction_hash(tx);
    return false;
  }

  uint64_t amount_in = 0;
  get_inputs_money_amount(tx, amount_in);
  uint64_t amount_out = get_outs_money_amount(tx);

  if (amount_in <= amount_out) {
    logger(ERROR) << "tx with wrong amounts: ins " << amount_in << ", outs " << amount_out << ", rejected for tx id= " << get_transaction_hash(tx);
    return false;
  }

  if (!keeped_by_block && get_object_blobsize(tx) >= m_blockchain_storage.get_current_comulative_blocksize_limit() - m_currency.minerTxBlobReservedSize()) {
    logger(ERROR) << "transaction is too big " << get_object_blobsize(tx) << ", maximum allowed size is " <<
      (m_blockchain_storage.get_current_comulative_blocksize_limit() - m_currency.minerTxBlobReservedSize());
    return false;
  }

  //check if tx use different key images
  if (!check_tx_inputs_keyimages_diff(tx)) {
    logger(ERROR) << "tx has a few inputs with identical keyimages";
    return false;
  }

  if (!checkMultisignatureInputsDiff(tx)) {
    logger(ERROR) << "tx has a few multisignature inputs with identical output indexes";
    return false;
  }

  return true;
}

bool core::check_tx_inputs_keyimages_diff(const Transaction& tx) {
  std::unordered_set<crypto::key_image> ki;
  for (const auto& in : tx.vin) {
    if (in.type() == typeid(TransactionInputToKey)) {
      if (!ki.insert(boost::get<TransactionInputToKey>(in).keyImage).second)
        return false;
    }
  }
  return true;
}

size_t core::get_blockchain_total_transactions() {
  return m_blockchain_storage.get_total_transactions();
}

//bool core::get_outs(uint64_t amount, std::list<crypto::public_key>& pkeys)
//{
//  return m_blockchain_storage.get_outs(amount, pkeys);
//}

bool core::add_new_tx(const Transaction& tx, const crypto::hash& tx_hash, const crypto::hash& tx_prefix_hash, size_t blob_size, tx_verification_context& tvc, bool keeped_by_block) {
  if (m_blockchain_storage.have_tx(tx_hash)) {
    logger(TRACE) << "tx " << tx_hash << " is already in blockchain";
    return true;
  }

  // It's not very good to lock on m_mempool here, because it's very hard to understand the order of locking
  // tx_memory_pool::m_transactions_lock, blockchain_storage::m_blockchain_lock, and core::m_incoming_tx_lock
  std::lock_guard<decltype(m_mempool)> lk(m_mempool);

  if (m_mempool.have_tx(tx_hash)) {
    logger(TRACE) << "tx " << tx_hash << " is already in transaction pool";
    return true;
  }

  return m_mempool.add_tx(tx, tx_hash, blob_size, tvc, keeped_by_block);
}

bool core::get_block_template(Block& b, const AccountPublicAddress& adr, difficulty_type& diffic, uint32_t& height, const blobdata& ex_nonce) {
  return m_blockchain_storage.create_block_template(b, adr, diffic, height, ex_nonce);
}

bool core::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp) {
  return m_blockchain_storage.find_blockchain_supplement(qblock_ids, resp);
}

bool core::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<Block, std::list<Transaction> > >& blocks, uint64_t& total_height, uint64_t& start_height, size_t max_count) {
  return m_blockchain_storage.find_blockchain_supplement(qblock_ids, blocks, total_height, start_height, max_count);
}

void core::print_blockchain(uint64_t start_index, uint64_t end_index) {
  m_blockchain_storage.print_blockchain(start_index, end_index);
}

void core::print_blockchain_index() {
  m_blockchain_storage.print_blockchain_index();
}

void core::print_blockchain_outs(const std::string& file) {
  m_blockchain_storage.print_blockchain_outs(file);
}

bool core::get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) {
  return m_blockchain_storage.get_random_outs_for_amounts(req, res);
}

bool core::get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) {
  return m_blockchain_storage.get_tx_outputs_gindexs(tx_id, indexs);
}

void core::pause_mining() {
  m_miner->pause();
}

void core::update_block_template_and_resume_mining() {
  update_miner_block_template();
  m_miner->resume();
}

bool core::handle_block_found(Block& b) {
  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  handle_incoming_block(b, bvc, true, true);

  if (bvc.m_verifivation_failed) {
    logger(ERROR) << "mined block failed verification";
  }

  return bvc.m_added_to_main_chain;
}

void core::on_synchronized() {
  m_miner->on_synchronized();
}
//-----------------------------------------------------------------------------------------------
bool core::getPoolChanges(const crypto::hash& tailBlockId, const std::vector<crypto::hash>& knownTxsIds,
                          std::vector<Transaction>& addedTxs, std::vector<crypto::hash>& deletedTxsIds) {
  return m_blockchain_storage.getPoolChanges(tailBlockId, knownTxsIds, addedTxs, deletedTxsIds);
}
//-----------------------------------------------------------------------------------------------
void core::getPoolChanges(const std::vector<crypto::hash>& knownTxsIds, std::vector<Transaction>& addedTxs,
                          std::vector<crypto::hash>& deletedTxsIds) {
  m_blockchain_storage.getPoolChanges(knownTxsIds, addedTxs, deletedTxsIds);
}
//-----------------------------------------------------------------------------------------------
bool core::handle_incoming_block_blob(const blobdata& block_blob, block_verification_context& bvc, bool control_miner, bool relay_block) {
  if (block_blob.size() > m_currency.maxBlockBlobSize()) {
    logger(INFO) << "WRONG BLOCK BLOB, too big size " << block_blob.size() << ", rejected";
    bvc.m_verifivation_failed = true;
    return false;
  }

  Block b;
  if (!parse_and_validate_block_from_blob(block_blob, b)) {
    logger(INFO) << "Failed to parse and validate new block";
    bvc.m_verifivation_failed = true;
    return false;
  }

  return handle_incoming_block(b, bvc, control_miner, relay_block);
}

bool core::handle_incoming_block(const Block& b, block_verification_context& bvc, bool control_miner, bool relay_block) {
  if (control_miner) {
    pause_mining();
  }

  m_blockchain_storage.add_new_block(b, bvc);

  if (control_miner) {
    update_block_template_and_resume_mining();
  }

  if (relay_block && bvc.m_added_to_main_chain) {
    std::list<crypto::hash> missed_txs;
    std::list<Transaction> txs;
    m_blockchain_storage.get_transactions(b.txHashes, txs, missed_txs);
    if (!missed_txs.empty() && m_blockchain_storage.get_block_id_by_height(get_block_height(b)) != get_block_hash(b)) {
      logger(INFO) << "Block added, but it seems that reorganize just happened after that, do not relay this block";
    } else {
      if (!(txs.size() == b.txHashes.size() && missed_txs.empty())) {
        logger(ERROR, BRIGHT_RED) << "can't find some transactions in found block:" <<
          get_block_hash(b) << " txs.size()=" << txs.size() << ", b.txHashes.size()=" << b.txHashes.size() << ", missed_txs.size()" << missed_txs.size(); return false;
      }

      NOTIFY_NEW_BLOCK::request arg;
      arg.hop = 0;
      arg.current_blockchain_height = m_blockchain_storage.get_current_blockchain_height();
      bool r = block_to_blob(b, arg.b.block);
      if (!(r)) { logger(ERROR, BRIGHT_RED) << "failed to serialize block"; return false; }
      for (auto& tx : txs) {
        arg.b.txs.push_back(t_serializable_object_to_blob(tx));
      }

      m_pprotocol->relay_block(arg);
    }
  }

  return true;
}

crypto::hash core::get_tail_id() {
  return m_blockchain_storage.get_tail_id();
}

size_t core::get_pool_transactions_count() {
  return m_mempool.get_transactions_count();
}

bool core::have_block(const crypto::hash& id) {
  return m_blockchain_storage.have_block(id);
}

bool core::parse_tx_from_blob(Transaction& tx, crypto::hash& tx_hash, crypto::hash& tx_prefix_hash, const blobdata& blob) {
  return parse_and_validate_tx_from_blob(blob, tx, tx_hash, tx_prefix_hash);
}

bool core::check_tx_syntax(const Transaction& tx) {
  return true;
}

std::vector<Transaction> core::getPoolTransactions() {
  std::list<Transaction> txs;
  m_mempool.get_transactions(txs);

  std::vector<Transaction> result;
  for (auto& tx : txs) {
    result.emplace_back(std::move(tx));
  }
  return result;
}

bool core::get_short_chain_history(std::list<crypto::hash>& ids) {
  return m_blockchain_storage.get_short_chain_history(ids);
}

bool core::handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) {
  return m_blockchain_storage.handle_get_objects(arg, rsp);
}

crypto::hash core::getBlockIdByHeight(uint64_t height) {
  return m_blockchain_storage.get_block_id_by_height(height);
}

bool core::getBlockByHash(const crypto::hash &h, Block &blk) {
  return m_blockchain_storage.get_block_by_hash(h, blk);
}

//void core::get_all_known_block_ids(std::list<crypto::hash> &main, std::list<crypto::hash> &alt, std::list<crypto::hash> &invalid) {
//  m_blockchain_storage.get_all_known_block_ids(main, alt, invalid);
//}

std::string core::print_pool(bool short_format) {
  return m_mempool.print_pool(short_format);
}

bool core::update_miner_block_template() {
  m_miner->on_block_chain_update();
  return true;
}

bool core::on_idle() {
  if (!m_starter_message_showed) {
    logger(INFO) << ENDL << "**********************************************************************" << ENDL
      << "The daemon will start synchronizing with the network. It may take up to several hours." << ENDL
      << ENDL
      << "You can set the level of process detailization* through \"set_log <level>\" command*, where <level> is between 0 (no details) and 4 (very verbose)." << ENDL
      << ENDL
      << "Use \"help\" command to see the list of available commands." << ENDL
      << ENDL
      << "Note: in case you need to interrupt the process, use \"exit\" command. Otherwise, the current progress won't be saved." << ENDL
      << "**********************************************************************";
    m_starter_message_showed = true;
  }

  m_miner->on_idle();
  m_mempool.on_idle();
  return true;
}

bool core::addObserver(ICoreObserver* observer) {
  return m_observerManager.add(observer);
}

bool core::removeObserver(ICoreObserver* observer) {
  return m_observerManager.remove(observer);
}

void core::blockchainUpdated() {
  m_observerManager.notify(&ICoreObserver::blockchainUpdated);
}

  void core::txDeletedFromPool() {
    poolUpdated();
  }

  void core::poolUpdated() {
    m_observerManager.notify(&ICoreObserver::poolUpdated);
  }

  bool core::queryBlocks(const std::list<crypto::hash>& knownBlockIds, uint64_t timestamp,
  uint64_t& resStartHeight, uint64_t& resCurrentHeight, uint64_t& resFullOffset, std::list<BlockFullInfo>& entries) {

  LockedBlockchainStorage lbs(m_blockchain_storage);

  uint64_t currentHeight = lbs->get_current_blockchain_height();
  uint64_t startOffset = 0;

  if (!lbs->find_blockchain_supplement(knownBlockIds, startOffset)) {
    return false;
  }

  uint64_t startFullOffset = 0;

  if (!lbs->getLowerBound(timestamp, startOffset, startFullOffset))
    startFullOffset = startOffset;

  resFullOffset = startFullOffset;

  if (startOffset != startFullOffset) {
    std::list<crypto::hash> blockIds;
    if (!lbs->getBlockIds(startOffset, std::min(uint64_t(BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT), startFullOffset - startOffset), blockIds)) {
      return false;
    }

    for (const auto& id : blockIds) {
      entries.push_back(BlockFullInfo());
      entries.back().block_id = id;
    }
  }

  auto blocksLeft = std::min(BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT - entries.size(), size_t(BLOCKS_SYNCHRONIZING_DEFAULT_COUNT));

  if (blocksLeft) {
    std::list<Block> blocks;
    lbs->get_blocks(startFullOffset, blocksLeft, blocks);

    for (auto& b : blocks) {
      BlockFullInfo item;

      item.block_id = get_block_hash(b);

      if (b.timestamp >= timestamp) {
        // query transactions
        std::list<Transaction> txs;
        std::list<crypto::hash> missedTxs;
        lbs->get_transactions(b.txHashes, txs, missedTxs);

        // fill data
        block_complete_entry& completeEntry = item;
        completeEntry.block = block_to_blob(b);
        for (auto& tx : txs) {
          completeEntry.txs.push_back(tx_to_blob(tx));
        }
      }

      entries.push_back(std::move(item));
    }
  }

  resCurrentHeight = currentHeight;
  resStartHeight = startOffset;

  return true;
}

bool core::getBackwardBlocksSizes(uint64_t fromHeight, std::vector<size_t>& sizes, size_t count) {
  return m_blockchain_storage.get_backward_blocks_sizes(fromHeight, sizes, count);
}

bool core::getBlockSize(const crypto::hash& hash, size_t& size) {
  return m_blockchain_storage.getBlockSize(hash, size);
}

bool core::getAlreadyGeneratedCoins(const crypto::hash& hash, uint64_t& generatedCoins) {
  return m_blockchain_storage.getAlreadyGeneratedCoins(hash, generatedCoins);
}

bool core::getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee,
                        bool penalizeFee, uint64_t& reward, int64_t& emissionChange) {
  return m_currency.getBlockReward(medianSize, currentBlockSize, alreadyGeneratedCoins, fee, penalizeFee, reward, emissionChange);
}

bool core::scanOutputkeysForIndices(const TransactionInputToKey& txInToKey, std::list<std::pair<crypto::hash, size_t>>& outputReferences) {
  struct outputs_visitor
  {
    std::list<std::pair<crypto::hash, size_t>>& m_resultsCollector;
    outputs_visitor(std::list<std::pair<crypto::hash, size_t>>& resultsCollector):m_resultsCollector(resultsCollector){}
    bool handle_output(const Transaction& tx, const TransactionOutput& out, size_t transactionOutputIndex)
    {
      m_resultsCollector.push_back(std::make_pair(get_transaction_hash(tx), transactionOutputIndex));
      return true;
    }
  };
    
  outputs_visitor vi(outputReferences);
    
  return m_blockchain_storage.scan_outputkeys_for_indexes(txInToKey, vi);
}

bool core::getBlockDifficulty(uint64_t height, difficulty_type& difficulty) {
  difficulty = m_blockchain_storage.block_difficulty(height);
  return true;
}

bool core::getBlockContainingTx(const crypto::hash& txId, crypto::hash& blockId, uint64_t& blockHeight) {
  return m_blockchain_storage.getBlockContainingTx(txId, blockId, blockHeight);
}

bool core::getMultisigOutputReference(const TransactionInputMultisignature& txInMultisig, std::pair<crypto::hash, size_t>& outputReference) {
  return m_blockchain_storage.getMultisigOutputReference(txInMultisig, outputReference);
}

}
