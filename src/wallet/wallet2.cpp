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

#include "wallet2.h"

#include <future>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/utility/value_init.hpp>

// epee
#include "include_base_utils.h"
#include "misc_language.h"
#include "profile_tools.h"

#include "common/boost_serialization_helper.h"
#include "crypto/crypto.h"
#include "cryptonote_core/AccountKVSerialization.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_protocol/blobdatatype.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "serialization/binary_utils.h"

using namespace cryptonote;
using namespace epee;

namespace
{
void do_prepare_file_names(const std::string& file_path, std::string& keys_file, std::string& wallet_file)
{
  keys_file = file_path;
  wallet_file = file_path;
  boost::system::error_code e;
  if(string_tools::get_extension(keys_file) == "keys")
  {//provided keys file name
    wallet_file = string_tools::cut_off_extension(wallet_file);
  }else
  {//provided wallet file name
    keys_file += ".keys";
  }
}
} //namespace

namespace tools
{
//----------------------------------------------------------------------------------------------------
void wallet2::init(const std::string& daemon_address)
{
  m_daemon_address = daemon_address;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::processNewTransaction(TxQueue& queue, const cryptonote::Transaction& tx, uint64_t height, const crypto::hash& bl_id) {
  process_unconfirmed(tx);

  std::vector<tx_extra_field> tx_extra_fields;
  if(!parse_tx_extra(tx.extra, tx_extra_fields))
  {
    // Extra may only be partially parsed, it's OK if tx_extra_fields contains public key
    LOG_PRINT_L0("Transaction extra has unsupported format: " << get_transaction_hash(tx));
  }

  tx_extra_pub_key pub_key_field;
  if(!find_tx_extra_field_by_type(tx_extra_fields, pub_key_field))
  {
    LOG_PRINT_L0("Public key wasn't found in the transaction extra. Skipping transaction " << get_transaction_hash(tx));
    if(0 != m_callback)
      m_callback->on_skip_transaction(height, tx);
    return false;
  }

  TxItem txItem = { tx, height, bl_id, pub_key_field.pub_key, std::move(tx_extra_fields) };
  queue.push(std::unique_ptr<TxItem>(new TxItem(std::move(txItem))));
  return true;
}

//----------------------------------------------------------------------------------------------------
void wallet2::processCheckedTransaction(const TxItem& item) {

  const cryptonote::Transaction& tx = item.tx;
  const std::vector<size_t>& outs = item.outs;
  const std::vector<tx_extra_field>& tx_extra_fields = item.txExtraFields;

  uint64_t tx_money_got_in_outs = item.txMoneyGotInOuts;
  uint64_t height = item.height;
<<<<<<< HEAD


=======


>>>>>>> 4363a9f1001893c80ee2435399836cfe43b3014e
  if (!outs.empty() && tx_money_got_in_outs)
  {
    //good news - got money! take care about it
    //usually we have only one transfer for user in transaction
    cryptonote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request req = AUTO_VAL_INIT(req);
    cryptonote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response res = AUTO_VAL_INIT(res);
    req.txid = get_transaction_hash(tx);
    bool r = net_utils::invoke_http_bin_remote_command2(m_daemon_address + "/get_o_indexes.bin", req, res, m_http_client, WALLET_RCP_CONNECTION_TIMEOUT);
    THROW_WALLET_EXCEPTION_IF(!r, error::no_connection_to_daemon, "get_o_indexes.bin");
    THROW_WALLET_EXCEPTION_IF(res.status == CORE_RPC_STATUS_BUSY, error::daemon_busy, "get_o_indexes.bin");
    THROW_WALLET_EXCEPTION_IF(res.status != CORE_RPC_STATUS_OK, error::get_out_indices_error, res.status);
    THROW_WALLET_EXCEPTION_IF(res.o_indexes.size() != tx.vout.size(), error::wallet_internal_error,
      "transactions outputs size=" + std::to_string(tx.vout.size()) +
      " not match with COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES response size=" + std::to_string(res.o_indexes.size()));

    for (size_t o : outs) {
      THROW_WALLET_EXCEPTION_IF(tx.vout.size() <= o, error::wallet_internal_error, "wrong out in transaction: internal index=" +
        std::to_string(o) + ", total_outs=" + std::to_string(tx.vout.size()));

      m_transfers.push_back(boost::value_initialized<transfer_details>());
      transfer_details& td = m_transfers.back();
      td.m_block_height = height;
      td.m_internal_output_index = o;
      td.m_global_output_index = res.o_indexes[o];
      td.m_tx = tx;
      td.m_spent = false;
      cryptonote::KeyPair in_ephemeral;
      cryptonote::generate_key_image_helper(m_account.get_keys(), item.txPubKey, o, in_ephemeral, td.m_key_image);
      THROW_WALLET_EXCEPTION_IF(in_ephemeral.pub != boost::get<cryptonote::TransactionOutputToKey>(tx.vout[o].target).key,
        error::wallet_internal_error, "key_image generated ephemeral public key not matched with output_key");

      m_key_images[td.m_key_image] = m_transfers.size() - 1;
      LOG_PRINT_L0("Received money: " << m_currency.formatAmount(td.amount()) << ", with tx: " << get_transaction_hash(tx));
      if (0 != m_callback) {
        m_callback->on_money_received(height, td.m_tx, td.m_internal_output_index);
      }
    }
  }

  uint64_t tx_money_spent_in_ins = 0;
  // check all outputs for spending (compare key images)
  BOOST_FOREACH(auto& in, tx.vin)
  {
    if (in.type() == typeid(cryptonote::TransactionInputToKey)) {
      auto it = m_key_images.find(boost::get<cryptonote::TransactionInputToKey>(in).keyImage);
      if (it != m_key_images.end()) {
        LOG_PRINT_L0("Spent money: " << m_currency.formatAmount(boost::get<cryptonote::TransactionInputToKey>(in).amount) <<
          ", with tx: " << get_transaction_hash(tx));
        tx_money_spent_in_ins += boost::get<cryptonote::TransactionInputToKey>(in).amount;
        transfer_details& td = m_transfers[it->second];
        td.m_spent = true;
        if (0 != m_callback)
          m_callback->on_money_spent(height, td.m_tx, td.m_internal_output_index, tx);
      }
    }
  }

  tx_extra_nonce extra_nonce;
  if (find_tx_extra_field_by_type(tx_extra_fields, extra_nonce))
  {
    crypto::hash payment_id;
    if (get_payment_id_from_tx_extra_nonce(extra_nonce.nonce, payment_id))
    {
      uint64_t received = (tx_money_spent_in_ins < tx_money_got_in_outs) ? tx_money_got_in_outs - tx_money_spent_in_ins : 0;
      if (0 < received && null_hash != payment_id)
      {
        payment_details payment;
        payment.m_tx_hash = cryptonote::get_transaction_hash(tx);
        payment.m_amount = received;
        payment.m_block_height = height;
        payment.m_unlock_time = tx.unlockTime;
        m_payments.emplace(payment_id, payment);
        LOG_PRINT_L2("Payment found: " << payment_id << " / " << payment.m_tx_hash << " / " << payment.m_amount);
      }
    }
  }

}

//----------------------------------------------------------------------------------------------------
void wallet2::process_unconfirmed(const cryptonote::Transaction& tx)
{
  auto unconf_it = m_unconfirmed_txs.find(get_transaction_hash(tx));
  if(unconf_it != m_unconfirmed_txs.end())
    m_unconfirmed_txs.erase(unconf_it);
}

//----------------------------------------------------------------------------------------------------
bool wallet2::addNewBlockchainEntry(const crypto::hash& bl_id, uint64_t start_height, uint64_t current_index)
<<<<<<< HEAD
{
  if (current_index < m_blockchain.size()) {
    if (bl_id != m_blockchain[current_index]) {
      //split detected here !!!
      THROW_WALLET_EXCEPTION_IF(current_index == start_height, error::wallet_internal_error,
        "wrong daemon response: split starts from the first block in response " + string_tools::pod_to_hex(bl_id) +
        " (height " + std::to_string(start_height) + "), local block id at this height: " +
        string_tools::pod_to_hex(m_blockchain[current_index]));

      detach_blockchain(current_index);
    } else {
      LOG_PRINT_L2("Block is already in blockchain: " << string_tools::pod_to_hex(bl_id));
      return false;
    }
  }

  THROW_WALLET_EXCEPTION_IF(current_index != m_blockchain.size(), error::wallet_internal_error,
    "current_index=" + std::to_string(current_index) + ", m_blockchain.size()=" + std::to_string(m_blockchain.size()));

  m_blockchain.push_back(bl_id);
  ++m_local_bc_height;

  if (0 != m_callback) {
    m_callback->on_new_block(current_index);
  }

  return true;
}

size_t wallet2::processNewBlockchainEntry(TxQueue& queue, const cryptonote::block_complete_entry& bche, const crypto::hash& bl_id, uint64_t height)
{
  size_t processedTransactions = 0;

=======
{
  if (current_index < m_blockchain.size()) {
    if (bl_id != m_blockchain[current_index]) {
      //split detected here !!!
      THROW_WALLET_EXCEPTION_IF(current_index == start_height, error::wallet_internal_error,
        "wrong daemon response: split starts from the first block in response " + string_tools::pod_to_hex(bl_id) +
        " (height " + std::to_string(start_height) + "), local block id at this height: " +
        string_tools::pod_to_hex(m_blockchain[current_index]));

      detach_blockchain(current_index);
    } else {
      LOG_PRINT_L2("Block is already in blockchain: " << string_tools::pod_to_hex(bl_id));
      return false;
    }
  }

  THROW_WALLET_EXCEPTION_IF(current_index != m_blockchain.size(), error::wallet_internal_error,
    "current_index=" + std::to_string(current_index) + ", m_blockchain.size()=" + std::to_string(m_blockchain.size()));

  m_blockchain.push_back(bl_id);
  ++m_local_bc_height;

  if (0 != m_callback) {
    m_callback->on_new_block(current_index);
  }

  return true;
}

size_t wallet2::processNewBlockchainEntry(TxQueue& queue, const cryptonote::block_complete_entry& bche, const crypto::hash& bl_id, uint64_t height)
{
  size_t processedTransactions = 0;

>>>>>>> 4363a9f1001893c80ee2435399836cfe43b3014e
  if (!bche.block.empty())
  {
    cryptonote::Block b;
    bool r = cryptonote::parse_and_validate_block_from_blob(bche.block, b);
    THROW_WALLET_EXCEPTION_IF(!r, error::block_parse_error, bche.block);
<<<<<<< HEAD

    //optimization: seeking only for blocks that are not older then the wallet creation time plus 1 day. 1 day is for possible user incorrect time setup
    if (b.timestamp + 60 * 60 * 24 > m_account.get_createtime())
    {
      TIME_MEASURE_START(miner_tx_handle_time);
      if(processNewTransaction(queue, b.minerTx, height, bl_id))
        ++processedTransactions;
      TIME_MEASURE_FINISH(miner_tx_handle_time);

      TIME_MEASURE_START(txs_handle_time);
      BOOST_FOREACH(auto& txblob, bche.txs)
      {
        cryptonote::Transaction tx;
        bool r = parse_and_validate_tx_from_blob(txblob, tx);
        THROW_WALLET_EXCEPTION_IF(!r, error::tx_parse_error, txblob);
        if(processNewTransaction(queue, tx, height, bl_id))
          ++processedTransactions;
      }
      TIME_MEASURE_FINISH(txs_handle_time);
      LOG_PRINT_L2("Processed block: " << bl_id << ", height " << height << ", " << miner_tx_handle_time + txs_handle_time << "(" << miner_tx_handle_time << "/" << txs_handle_time << ")ms");
    } else
    {
=======

    //optimization: seeking only for blocks that are not older then the wallet creation time plus 1 day. 1 day is for possible user incorrect time setup
    if (b.timestamp + 60 * 60 * 24 > m_account.get_createtime())
    {
      TIME_MEASURE_START(miner_tx_handle_time);
      if(processNewTransaction(queue, b.minerTx, height, bl_id))
        ++processedTransactions;
      TIME_MEASURE_FINISH(miner_tx_handle_time);

      TIME_MEASURE_START(txs_handle_time);
      BOOST_FOREACH(auto& txblob, bche.txs)
      {
        cryptonote::Transaction tx;
        bool r = parse_and_validate_tx_from_blob(txblob, tx);
        THROW_WALLET_EXCEPTION_IF(!r, error::tx_parse_error, txblob);
        if(processNewTransaction(queue, tx, height, bl_id))
          ++processedTransactions;
      }
      TIME_MEASURE_FINISH(txs_handle_time);
      LOG_PRINT_L2("Processed block: " << bl_id << ", height " << height << ", " << miner_tx_handle_time + txs_handle_time << "(" << miner_tx_handle_time << "/" << txs_handle_time << ")ms");
    } else
    {
>>>>>>> 4363a9f1001893c80ee2435399836cfe43b3014e
      LOG_PRINT_L2("Skipped block by timestamp, height: " << height << ", block time " << b.timestamp << ", account time " << m_account.get_createtime());
    }
  }

  return processedTransactions;
}


//----------------------------------------------------------------------------------------------------
void wallet2::get_short_chain_history(std::list<crypto::hash>& ids) const
{
  size_t i = 0;
  size_t current_multiplier = 1;
  size_t sz = m_blockchain.size();
  if(!sz)
    return;
  size_t current_back_offset = 1;
  bool genesis_included = false;
  while(current_back_offset < sz)
  {
    ids.push_back(m_blockchain[sz-current_back_offset]);
    if(sz-current_back_offset == 0)
      genesis_included = true;
    if(i < 10)
    {
      ++current_back_offset;
    }else
    {
      current_back_offset += current_multiplier *= 2;
    }
    ++i;
  }
  if(!genesis_included)
    ids.push_back(m_blockchain[0]);
}

//----------------------------------------------------------------------------------------------------
size_t wallet2::updateBlockchain(const cryptonote::COMMAND_RPC_QUERY_BLOCKS::response& res)
{
  size_t blocks_added = 0;
  size_t current_index = res.start_height;

  // update local blockchain
  for (const auto& item : res.items) {
    if (addNewBlockchainEntry(item.block_id, res.start_height, current_index))
      ++blocks_added;
    ++current_index;
  }

  return blocks_added;
}

//----------------------------------------------------------------------------------------------------
void wallet2::processTransactions(const cryptonote::COMMAND_RPC_QUERY_BLOCKS::response& res)
{
  size_t checkingThreads = std::thread::hardware_concurrency();

  if (checkingThreads == 0)
    checkingThreads = 4;

  std::vector<std::future<void>> futures;

  TxQueue incomingQueue(checkingThreads * 2);
  TxQueue checkedQueue(checkingThreads * 2);

  std::atomic<size_t> inputTx(0);
  std::atomic<size_t> checkedTx(0);

  futures.push_back(std::async(std::launch::async, [&] {
    try {
      size_t current_index = res.start_height;
      for (const auto& item : res.items) {
        inputTx += processNewBlockchainEntry(incomingQueue, item, item.block_id, current_index);
        ++current_index;
      }
      incomingQueue.close();
    } catch (...) {
      LOG_ERROR("Exception in pushing thread!");
      incomingQueue.close();
      throw;
    }
  }));

  GroupClose<TxQueue> queueClose(checkedQueue, checkingThreads);

  for (size_t i = 0; i < checkingThreads; ++i) {
    futures.push_back(std::async(std::launch::async, [&] {
      TxQueueItem item;
      while (incomingQueue.pop(item)) {
        ++checkedTx;
        lookup_acc_outs(m_account.get_keys(), item->tx, item->txPubKey, item->outs, item->txMoneyGotInOuts);
        checkedQueue.push(std::move(item));
      }
      queueClose.close();
    }));
  }

  size_t txCount = 0;

  TxQueueItem item;
  while (checkedQueue.pop(item)) {
    processCheckedTransaction(*item);
    ++txCount;
  }

  for (auto& f : futures) {
    f.get();
<<<<<<< HEAD
  }

  if (checkedQueue.size() > 0 || incomingQueue.size() > 0) {
    LOG_ERROR("ERROR! Queues not empty. Incoming: " << incomingQueue.size() << " Checked: " << checkedQueue.size());
  }

  if (inputTx != txCount) {
    LOG_ERROR("Failed to process some transactions. Incoming: " << inputTx << " Processed: " << txCount);
=======
>>>>>>> 4363a9f1001893c80ee2435399836cfe43b3014e
  }

  if (checkedQueue.size() > 0 || incomingQueue.size() > 0) {
    LOG_ERROR("ERROR! Queues not empty. Incoming: " << incomingQueue.size() << " Checked: " << checkedQueue.size());
  }

  if (inputTx != txCount) {
    LOG_ERROR("Failed to process some transactions. Incoming: " << inputTx << " Processed: " << txCount);
  }
}

//----------------------------------------------------------------------------------------------------
cryptonote::COMMAND_RPC_QUERY_BLOCKS::response wallet2::queryBlocks(epee::net_utils::http::http_simple_client& client)
{
  cryptonote::COMMAND_RPC_QUERY_BLOCKS::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_QUERY_BLOCKS::response res = AUTO_VAL_INIT(res);

  get_short_chain_history(req.block_ids);
  req.timestamp = m_account.get_createtime() - 60 * 60 * 24; // get full blocks starting from wallet creation time minus 1 day

  bool r = net_utils::invoke_http_bin_remote_command2(m_daemon_address + "/queryblocks.bin", req, res, client, WALLET_RCP_CONNECTION_TIMEOUT);
  THROW_WALLET_EXCEPTION_IF(!r, error::no_connection_to_daemon, "queryblocks.bin");
  THROW_WALLET_EXCEPTION_IF(res.status == CORE_RPC_STATUS_BUSY, error::daemon_busy, "queryblocks.bin");
  THROW_WALLET_EXCEPTION_IF(res.status != CORE_RPC_STATUS_OK, error::get_blocks_error, res.status);
  THROW_WALLET_EXCEPTION_IF(m_blockchain.size() <= res.start_height, error::wallet_internal_error,
    "wrong daemon response: m_start_height=" + std::to_string(res.start_height) +
    " not less than local blockchain size=" + std::to_string(m_blockchain.size()));

  return res;
}

<<<<<<< HEAD
//----------------------------------------------------------------------------------------------------
cryptonote::COMMAND_RPC_QUERY_BLOCKS::response wallet2::queryBlocks(epee::net_utils::http::http_simple_client& client)
{
  cryptonote::COMMAND_RPC_QUERY_BLOCKS::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_QUERY_BLOCKS::response res = AUTO_VAL_INIT(res);

  get_short_chain_history(req.block_ids);
  req.timestamp = m_account.get_createtime() - 60 * 60 * 24; // get full blocks starting from wallet creation time minus 1 day

  bool r = net_utils::invoke_http_bin_remote_command2(m_daemon_address + "/queryblocks.bin", req, res, client, WALLET_RCP_CONNECTION_TIMEOUT);
  THROW_WALLET_EXCEPTION_IF(!r, error::no_connection_to_daemon, "queryblocks.bin");
  THROW_WALLET_EXCEPTION_IF(res.status == CORE_RPC_STATUS_BUSY, error::daemon_busy, "queryblocks.bin");
  THROW_WALLET_EXCEPTION_IF(res.status != CORE_RPC_STATUS_OK, error::get_blocks_error, res.status);
  THROW_WALLET_EXCEPTION_IF(m_blockchain.size() <= res.start_height, error::wallet_internal_error,
    "wrong daemon response: m_start_height=" + std::to_string(res.start_height) +
    " not less than local blockchain size=" + std::to_string(m_blockchain.size()));

  return res;
}

=======
>>>>>>> 4363a9f1001893c80ee2435399836cfe43b3014e
//----------------------------------------------------------------------------------------------------
void wallet2::refresh()
{
  size_t blocks_fetched = 0;
  refresh(blocks_fetched);
}
//----------------------------------------------------------------------------------------------------
void wallet2::refresh(size_t & blocks_fetched)
{
  bool received_money = false;
  refresh(blocks_fetched, received_money);
}
//----------------------------------------------------------------------------------------------------
void wallet2::refresh(size_t & blocks_fetched, bool& received_money)
{
  received_money = false;
  blocks_fetched = 0;
  size_t added_blocks = 0;
  size_t try_count = 0;
  crypto::hash last_tx_hash_id = m_transfers.size() ? get_transaction_hash(m_transfers.back().m_tx) : null_hash;

  std::future<void> processingTask;

  epee::net_utils::http::http_simple_client queryClient;

  auto r = connectClient(queryClient);
  THROW_WALLET_EXCEPTION_IF(!r, error::no_connection_to_daemon, "refresh");

  auto startTime = std::chrono::high_resolution_clock::now();

  while(m_run.load(std::memory_order_relaxed))
  {
    try
    {
      auto res = std::make_shared<cryptonote::COMMAND_RPC_QUERY_BLOCKS::response>(queryBlocks(queryClient));
      
      if (processingTask.valid())
        processingTask.get(); // sync with transaction processing

      added_blocks = updateBlockchain(*res);
      
      if (!added_blocks) {
        break;
      }

      blocks_fetched += added_blocks;

      bool hasFullBlocks = std::any_of(res->items.begin(), res->items.end(), 
        [](const cryptonote::COMMAND_RPC_QUERY_BLOCKS::response_item& ri) { return !ri.block.empty(); });

      if (hasFullBlocks) {
        processingTask = std::async(std::launch::async, [res, this] { processTransactions(*res); });
      }
    }
    catch (const std::exception&)
    {
      blocks_fetched += added_blocks;
      if(try_count < 3)
      {
        LOG_PRINT_L1("Another try pull_blocks (try_count=" << try_count << ")...");
        ++try_count;
      }
      else
      {
        LOG_ERROR("pull_blocks failed, try_count=" << try_count);
        throw;
      }
    }
  }

  if (processingTask.valid())
    processingTask.get();

  auto duration = std::chrono::high_resolution_clock::now() - startTime;

  if(last_tx_hash_id != (m_transfers.size() ? get_transaction_hash(m_transfers.back().m_tx) : null_hash))
    received_money = true;

  LOG_PRINT_L1("Refresh done, blocks received: " << blocks_fetched <<
     ", balance: " << m_currency.formatAmount(balance()) <<
     ", unlocked: " << m_currency.formatAmount(unlocked_balance()));
  LOG_PRINT_L1("Time: " << std::chrono::duration<float>(duration).count() << "s");
}
//----------------------------------------------------------------------------------------------------
bool wallet2::refresh(size_t & blocks_fetched, bool& received_money, bool& ok)
{
  try
  {
    refresh(blocks_fetched, received_money);
    ok = true;
  }
  catch (...)
  {
    ok = false;
  }
  return ok;
}
//----------------------------------------------------------------------------------------------------
void wallet2::detach_blockchain(uint64_t height)
{
  LOG_PRINT_L0("Detaching blockchain on height " << height);
  size_t transfers_detached = 0;

  auto it = std::find_if(m_transfers.begin(), m_transfers.end(), [&](const transfer_details& td){return td.m_block_height >= height;});
  size_t i_start = it - m_transfers.begin();

  for(size_t i = i_start; i!= m_transfers.size();i++)
  {
    auto it_ki = m_key_images.find(m_transfers[i].m_key_image);
    THROW_WALLET_EXCEPTION_IF(it_ki == m_key_images.end(), error::wallet_internal_error, "key image not found");
    m_key_images.erase(it_ki);
    ++transfers_detached;
  }
  m_transfers.erase(it, m_transfers.end());

  size_t blocks_detached = m_blockchain.end() - (m_blockchain.begin()+height);
  m_blockchain.erase(m_blockchain.begin()+height, m_blockchain.end());
  m_local_bc_height -= blocks_detached;

  for (auto it = m_payments.begin(); it != m_payments.end(); )
  {
    if(height <= it->second.m_block_height)
      it = m_payments.erase(it);
    else
      ++it;
  }

  LOG_PRINT_L0("Detached blockchain on height " << height << ", transfers detached " << transfers_detached << ", blocks detached " << blocks_detached);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::deinit()
{
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::clear()
{
  m_blockchain.clear();
  m_transfers.clear();
<<<<<<< HEAD
=======
  m_blockchain.push_back(m_currency.genesisBlockHash());
>>>>>>> 4363a9f1001893c80ee2435399836cfe43b3014e
  m_local_bc_height = 1;
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::store_keys(const std::string& keys_file_name, const std::string& password)
{
  std::string account_data;
  AccountBaseSerializer<true> accountSerializer(m_account);
  bool r = epee::serialization::store_t_to_binary(accountSerializer, account_data);
  CHECK_AND_ASSERT_MES(r, false, "failed to serialize wallet keys");
  wallet2::keys_file_data keys_file_data = boost::value_initialized<wallet2::keys_file_data>();

  crypto::chacha8_key key;
  crypto::cn_context cn_context;
  crypto::generate_chacha8_key(cn_context, password, key);
  std::string cipher;
  cipher.resize(account_data.size());
  keys_file_data.iv = crypto::rand<crypto::chacha8_iv>();
  crypto::chacha8(account_data.data(), account_data.size(), key, keys_file_data.iv, &cipher[0]);
  keys_file_data.account_data = cipher;

  std::string buf;
  r = ::serialization::dump_binary(keys_file_data, buf);
  r = r && epee::file_io_utils::save_string_to_file(keys_file_name, buf); //and never touch wallet_keys_file again, only read
  CHECK_AND_ASSERT_MES(r, false, "failed to generate wallet keys file " << keys_file_name);

  return true;
}
//----------------------------------------------------------------------------------------------------
namespace
{
  bool verify_keys(const crypto::secret_key& sec, const crypto::public_key& expected_pub)
  {
    crypto::public_key pub;
    bool r = crypto::secret_key_to_public_key(sec, pub);
    return r && expected_pub == pub;
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::load_keys(const std::string& keys_file_name, const std::string& password)
{
  wallet2::keys_file_data keys_file_data;
  std::string buf;
  bool r = epee::file_io_utils::load_file_to_string(keys_file_name, buf);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, keys_file_name);
  r = ::serialization::parse_binary(buf, keys_file_data);
  THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "internal error: failed to deserialize \"" + keys_file_name + '\"');

  crypto::chacha8_key key;
  crypto::cn_context cn_context;
  crypto::generate_chacha8_key(cn_context, password, key);
  std::string account_data;
  account_data.resize(keys_file_data.account_data.size());
  crypto::chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);

  const cryptonote::account_keys& keys = m_account.get_keys();
  AccountBaseSerializer<false> accountSerializer(m_account);
  r = epee::serialization::load_t_from_binary(accountSerializer, account_data);
  r = r && verify_keys(keys.m_view_secret_key,  keys.m_account_address.m_viewPublicKey);
  r = r && verify_keys(keys.m_spend_secret_key, keys.m_account_address.m_spendPublicKey);
  THROW_WALLET_EXCEPTION_IF(!r, error::invalid_password);
}
//----------------------------------------------------------------------------------------------------
void wallet2::generate(const std::string& wallet_, const std::string& password)
{
  clear();
  prepare_file_names(wallet_);

  boost::system::error_code ignored_ec;
  THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_wallet_file, ignored_ec), error::file_exists, m_wallet_file);
  THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_keys_file,   ignored_ec), error::file_exists, m_keys_file);

  m_account.generate();
  m_account_public_address = m_account.get_keys().m_account_address;

  bool r = store_keys(m_keys_file, password);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, m_keys_file);

  r = file_io_utils::save_string_to_file(m_wallet_file + ".address.txt", m_currency.accountAddressAsString(m_account));
  if(!r) LOG_PRINT_RED_L0("String with address text not saved");

  m_blockchain.push_back(m_currency.genesisBlockHash());

  store();
}
//----------------------------------------------------------------------------------------------------
void wallet2::wallet_exists(const std::string& file_path, bool& keys_file_exists, bool& wallet_file_exists)
{
  std::string keys_file, wallet_file;
  do_prepare_file_names(file_path, keys_file, wallet_file);

  boost::system::error_code ignore;
  keys_file_exists = boost::filesystem::exists(keys_file, ignore);
  wallet_file_exists = boost::filesystem::exists(wallet_file, ignore);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::parse_payment_id(const std::string& payment_id_str, crypto::hash& payment_id) {
  cryptonote::blobdata payment_id_data;
  if (!epee::string_tools::parse_hexstr_to_binbuff(payment_id_str, payment_id_data)) {
    return false;
  }

  if (sizeof(crypto::hash) != payment_id_data.size()) {
    return false;
  }

  payment_id = *reinterpret_cast<const crypto::hash*>(payment_id_data.data());
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::prepare_file_names(const std::string& file_path)
{
  do_prepare_file_names(file_path, m_keys_file, m_wallet_file);
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::check_connection()
{
  if(m_http_client.is_connected())
    return true;
  return connectClient(m_http_client);
}

//----------------------------------------------------------------------------------------------------
bool wallet2::connectClient(epee::net_utils::http::http_simple_client& client) {
  net_utils::http::url_content u;
  net_utils::parse_url(m_daemon_address, u);
  if (!u.port)
    u.port = RPC_DEFAULT_PORT;
  return client.connect(u.host, std::to_string(u.port), WALLET_RCP_CONNECTION_TIMEOUT);
}

//----------------------------------------------------------------------------------------------------
void wallet2::load(const std::string& wallet_, const std::string& password)
{
  clear();
  prepare_file_names(wallet_);

  boost::system::error_code e;
  bool exists = boost::filesystem::exists(m_keys_file, e);
  THROW_WALLET_EXCEPTION_IF(e || !exists, error::file_not_found, m_keys_file);

  load_keys(m_keys_file, password);
  LOG_PRINT_L0("Loaded wallet keys file, with public address: " << m_currency.accountAddressAsString(m_account));

  //keys loaded ok!
  //try to load wallet file. but even if we failed, it is not big problem
  if(!boost::filesystem::exists(m_wallet_file, e) || e)
  {
    LOG_PRINT_L0("file not found: " << m_wallet_file << ", starting with empty blockchain");
    m_account_public_address = m_account.get_keys().m_account_address;
  } else {
    bool r = tools::unserialize_obj_from_file(*this, m_wallet_file);
    THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, m_wallet_file);
    THROW_WALLET_EXCEPTION_IF(
      m_account_public_address.m_spendPublicKey != m_account.get_keys().m_account_address.m_spendPublicKey ||
      m_account_public_address.m_viewPublicKey  != m_account.get_keys().m_account_address.m_viewPublicKey,
      error::wallet_files_doesnt_correspond, m_keys_file, m_wallet_file);
  }
<<<<<<< HEAD
  bool r = tools::unserialize_obj_from_file(*this, m_wallet_file);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, m_wallet_file);
  THROW_WALLET_EXCEPTION_IF(
    m_account_public_address.m_spendPublicKey != m_account.get_keys().m_account_address.m_spendPublicKey ||
    m_account_public_address.m_viewPublicKey  != m_account.get_keys().m_account_address.m_viewPublicKey,
    error::wallet_files_doesnt_correspond, m_keys_file, m_wallet_file);
=======
>>>>>>> 4363a9f1001893c80ee2435399836cfe43b3014e

  if (m_blockchain.empty()) {
    m_blockchain.push_back(m_currency.genesisBlockHash());
  } else {
    THROW_WALLET_EXCEPTION_IF(m_blockchain[0] != m_currency.genesisBlockHash(), error::wallet_internal_error,
      "Genesis block missmatch. You probably use wallet without testnet flag with blockchain from test network or vice versa");
  }

  m_local_bc_height = m_blockchain.size();
}
//----------------------------------------------------------------------------------------------------
void wallet2::store()
{
  bool r = tools::serialize_obj_to_file(*this, m_wallet_file);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, m_wallet_file);
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::unlocked_balance()
{
  uint64_t amount = 0;
  BOOST_FOREACH(transfer_details& td, m_transfers)
    if(!td.m_spent && is_transfer_unlocked(td))
      amount += td.amount();

  return amount;
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::balance()
{
  uint64_t amount = 0;
  BOOST_FOREACH(auto& td, m_transfers)
    if(!td.m_spent)
      amount += td.amount();


  BOOST_FOREACH(auto& utx, m_unconfirmed_txs)
    amount+= utx.second.m_change;

  return amount;
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_transfers(wallet2::transfer_container& incoming_transfers) const
{
  incoming_transfers = m_transfers;
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_payments(const crypto::hash& payment_id, std::list<wallet2::payment_details>& payments) const
{
  auto range = m_payments.equal_range(payment_id);
  std::for_each(range.first, range.second, [&payments](const payment_container::value_type& x) {
    payments.push_back(x.second);
  });
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_transfer_unlocked(const transfer_details& td) const
{
  if(!is_tx_spendtime_unlocked(td.m_tx.unlockTime))
    return false;

  if(td.m_block_height + DEFAULT_TX_SPENDABLE_AGE > m_blockchain.size())
    return false;

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_tx_spendtime_unlocked(uint64_t unlock_time) const {
  if (unlock_time < m_currency.maxBlockHeight()) {
    // interpret as block index
    return m_blockchain.size() - 1 + m_currency.lockedTxAllowedDeltaBlocks() >= unlock_time;
  } else {
    //interpret as time
    uint64_t current_time = static_cast<uint64_t>(time(NULL));
    return current_time + m_currency.lockedTxAllowedDeltaSeconds() >= unlock_time;
  }
  return false;
}
//----------------------------------------------------------------------------------------------------
namespace
{
  template<typename T>
  T pop_random_value(std::vector<T>& vec)
  {
    CHECK_AND_ASSERT_MES(!vec.empty(), T(), "Vector must be non-empty");

    size_t idx = crypto::rand<size_t>() % vec.size();
    T res = vec[idx];
    if (idx + 1 != vec.size())
    {
      vec[idx] = vec.back();
    }
    vec.resize(vec.size() - 1);

    return res;
  }
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::select_transfers(uint64_t needed_money, bool add_dust, uint64_t dust, std::list<transfer_container::iterator>& selected_transfers)
{
  std::vector<size_t> unused_transfers_indices;
  std::vector<size_t> unused_dust_indices;
  for (size_t i = 0; i < m_transfers.size(); ++i)
  {
    const transfer_details& td = m_transfers[i];
    if (!td.m_spent && is_transfer_unlocked(td))
    {
      if (dust < td.amount())
        unused_transfers_indices.push_back(i);
      else
        unused_dust_indices.push_back(i);
    }
  }

  bool select_one_dust = add_dust && !unused_dust_indices.empty();
  uint64_t found_money = 0;
  while (found_money < needed_money && (!unused_transfers_indices.empty() || !unused_dust_indices.empty()))
  {
    size_t idx;
    if (select_one_dust)
    {
      idx = pop_random_value(unused_dust_indices);
      select_one_dust = false;
    }
    else
    {
      idx = !unused_transfers_indices.empty() ? pop_random_value(unused_transfers_indices) : pop_random_value(unused_dust_indices);
    }

    transfer_container::iterator it = m_transfers.begin() + idx;
    selected_transfers.push_back(it);
    found_money += it->amount();
  }

  return found_money;
}
//----------------------------------------------------------------------------------------------------
void wallet2::add_unconfirmed_tx(const cryptonote::Transaction& tx, uint64_t change_amount)
{
  unconfirmed_transfer_details& utd = m_unconfirmed_txs[cryptonote::get_transaction_hash(tx)];
  utd.m_change = change_amount;
  utd.m_sent_time = time(NULL);
  utd.m_tx = tx;
}
//----------------------------------------------------------------------------------------------------
void wallet2::transfer(const std::vector<cryptonote::tx_destination_entry>& dsts, size_t fake_outputs_count,
                       uint64_t unlock_time, uint64_t fee, const std::vector<uint8_t>& extra, cryptonote::Transaction& tx)
{
  transfer(dsts, fake_outputs_count, unlock_time, fee, extra, detail::digit_split_strategy,
    tx_dust_policy(m_currency.defaultDustThreshold()), tx);
}
//----------------------------------------------------------------------------------------------------
void wallet2::transfer(const std::vector<cryptonote::tx_destination_entry>& dsts, size_t fake_outputs_count,
                       uint64_t unlock_time, uint64_t fee, const std::vector<uint8_t>& extra)
{
  cryptonote::Transaction tx;
  transfer(dsts, fake_outputs_count, unlock_time, fee, extra, tx);
}
//----------------------------------------------------------------------------------------------------
}
