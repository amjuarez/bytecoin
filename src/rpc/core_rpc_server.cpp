// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/foreach.hpp>
#include "include_base_utils.h"
using namespace epee;

#include "core_rpc_server.h"
#include "common/command_line.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "misc_language.h"
#include "crypto/hash.h"
#include "core_rpc_server_error_codes.h"

namespace cryptonote
{
  namespace
  {
    const command_line::arg_descriptor<std::string> arg_rpc_bind_ip   = {"rpc-bind-ip", "", "127.0.0.1"};
    const command_line::arg_descriptor<std::string> arg_rpc_bind_port = {"rpc-bind-port", "", std::to_string(RPC_DEFAULT_PORT)};
  }

  //-----------------------------------------------------------------------------------
  void core_rpc_server::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_rpc_bind_ip);
    command_line::add_arg(desc, arg_rpc_bind_port);
  }
  //------------------------------------------------------------------------------------------------------------------------------
  core_rpc_server::core_rpc_server(core& cr, nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core> >& p2p):m_core(cr), m_p2p(p2p)
  {}
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::handle_command_line(const boost::program_options::variables_map& vm)
  {
    m_bind_ip = command_line::get_arg(vm, arg_rpc_bind_ip);
    m_port = command_line::get_arg(vm, arg_rpc_bind_port);
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::init(const boost::program_options::variables_map& vm)
  {
    m_net_server.set_threads_prefix("RPC");
    bool r = handle_command_line(vm);
    CHECK_AND_ASSERT_MES(r, false, "Failed to process command line in core_rpc_server");
    return epee::http_server_impl_base<core_rpc_server, connection_context>::init(m_port, m_bind_ip);
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::check_core_ready()
  {
    if(!m_p2p.get_payload_object().is_synchronized())
    {
      return false;
    }
    if(m_p2p.get_payload_object().get_core().get_blockchain_storage().is_storing_blockchain())
    {
      return false;
    }
    return true;
  }
#define CHECK_CORE_READY() if(!check_core_ready()){res.status =  CORE_RPC_STATUS_BUSY;return true;}

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_height(const COMMAND_RPC_GET_HEIGHT::request& req, COMMAND_RPC_GET_HEIGHT::response& res, connection_context& cntx)
  {
    CHECK_CORE_READY();
    res.height = m_core.get_current_blockchain_height();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_info(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res, connection_context& cntx)
  {
    CHECK_CORE_READY();
    res.height = m_core.get_current_blockchain_height();
    res.difficulty = m_core.get_blockchain_storage().get_difficulty_for_next_block();
    res.tx_count = m_core.get_blockchain_storage().get_total_transactions() - res.height; //without coinbase
    res.tx_pool_size = m_core.get_pool_transactions_count();
    res.alt_blocks_count = m_core.get_blockchain_storage().get_alternative_blocks_count();
    uint64_t total_conn = m_p2p.get_connections_count();
    res.outgoing_connections_count = m_p2p.get_outgoing_connections_count();
    res.incoming_connections_count = total_conn - res.outgoing_connections_count;
    res.white_peerlist_size = m_p2p.get_peerlist_manager().get_white_peers_count();
    res.grey_peerlist_size = m_p2p.get_peerlist_manager().get_gray_peers_count();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_blocks(const COMMAND_RPC_GET_BLOCKS_FAST::request& req, COMMAND_RPC_GET_BLOCKS_FAST::response& res, connection_context& cntx)
  {
    CHECK_CORE_READY();
    std::list<std::pair<block, std::list<transaction> > > bs;
    if(!m_core.find_blockchain_supplement(req.block_ids, bs, res.current_height, res.start_height, COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT))
    {
      res.status = "Failed";
      return false;
    }

    BOOST_FOREACH(auto& b, bs)
    {
      res.blocks.resize(res.blocks.size()+1);
      res.blocks.back().block = block_to_blob(b.first);
      BOOST_FOREACH(auto& t, b.second)
      {
        res.blocks.back().txs.push_back(tx_to_blob(t));
      }
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_random_outs(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res, connection_context& cntx)
  {
    CHECK_CORE_READY();
    res.status = "Failed";
    if(!m_core.get_random_outs_for_amounts(req, res))
    {
      return true;
    }

    res.status = CORE_RPC_STATUS_OK;
    std::stringstream ss;
    typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount outs_for_amount;
    typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry out_entry;
    std::for_each(res.outs.begin(), res.outs.end(), [&](outs_for_amount& ofa)
    {
      ss << "[" << ofa.amount << "]:";
      CHECK_AND_ASSERT_MES(ofa.outs.size(), ;, "internal error: ofa.outs.size() is empty for amount " << ofa.amount);
      std::for_each(ofa.outs.begin(), ofa.outs.end(), [&](out_entry& oe)
          {
            ss << oe.global_amount_index << " ";
          });
      ss << ENDL;
    });
    std::string s = ss.str();
    LOG_PRINT_L2("COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS: " << ENDL << s);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_indexes(const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request& req, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response& res, connection_context& cntx)
  {
    CHECK_CORE_READY();
    bool r = m_core.get_tx_outputs_gindexs(req.txid, res.o_indexes);
    if(!r)
    {
      res.status = "Failed";
      return true;
    }
    res.status = CORE_RPC_STATUS_OK;
    LOG_PRINT_L2("COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES: [" << res.o_indexes.size() << "]");
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_transactions(const COMMAND_RPC_GET_TRANSACTIONS::request& req, COMMAND_RPC_GET_TRANSACTIONS::response& res, connection_context& cntx)
  {
    CHECK_CORE_READY();
    std::vector<crypto::hash> vh;
    BOOST_FOREACH(const auto& tx_hex_str, req.txs_hashes)
    {
      blobdata b;
      if(!string_tools::parse_hexstr_to_binbuff(tx_hex_str, b))
      {
        res.status = "Failed to parse hex representation of transaction hash";
        return true;
      }
      if(b.size() != sizeof(crypto::hash))
      {
        res.status = "Failed, size of data mismatch";
      }
      vh.push_back(*reinterpret_cast<const crypto::hash*>(b.data()));
    }
    std::list<crypto::hash> missed_txs;
    std::list<transaction> txs;
    bool r = m_core.get_transactions(vh, txs, missed_txs);
    if(!r)
    {
      res.status = "Failed";
      return true;
    }

    BOOST_FOREACH(auto& tx, txs)
    {
      blobdata blob = t_serializable_object_to_blob(tx);
      res.txs_as_hex.push_back(string_tools::buff_to_hex_nodelimer(blob));
    }

    BOOST_FOREACH(const auto& miss_tx, missed_txs)
    {
      res.missed_tx.push_back(string_tools::pod_to_hex(miss_tx));
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_send_raw_tx(const COMMAND_RPC_SEND_RAW_TX::request& req, COMMAND_RPC_SEND_RAW_TX::response& res, connection_context& cntx)
  {
    CHECK_CORE_READY();

    std::string tx_blob;
    if(!string_tools::parse_hexstr_to_binbuff(req.tx_as_hex, tx_blob))
    {
      LOG_PRINT_L0("[on_send_raw_tx]: Failed to parse tx from hexbuff: " << req.tx_as_hex);
      res.status = "Failed";
      return true;
    }

    cryptonote_connection_context fake_context = AUTO_VAL_INIT(fake_context);
    tx_verification_context tvc = AUTO_VAL_INIT(tvc);
    if(!m_core.handle_incoming_tx(tx_blob, tvc, false))
    {
      LOG_PRINT_L0("[on_send_raw_tx]: Failed to process tx");
      res.status = "Failed";
      return true;
    }

    if(tvc.m_verifivation_failed)
    {
      LOG_PRINT_L0("[on_send_raw_tx]: tx verification failed");
      res.status = "Failed";
      return true;
    }

    if(!tvc.m_should_be_relayed)
    {
      LOG_PRINT_L0("[on_send_raw_tx]: tx accepted, but not relayed");
      res.status = "Not relayed";
      return true;
    }


    NOTIFY_NEW_TRANSACTIONS::request r;
    r.txs.push_back(tx_blob);
    m_core.get_protocol()->relay_transactions(r, fake_context);
    //TODO: make sure that tx has reached other nodes here, probably wait to receive reflections from other nodes
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_start_mining(const COMMAND_RPC_START_MINING::request& req, COMMAND_RPC_START_MINING::response& res, connection_context& cntx)
  {
    CHECK_CORE_READY();
    account_public_address adr;
    if(!get_account_address_from_str(adr, req.miner_address))
    {
      res.status = "Failed, wrong address";
      return true;
    }

    boost::thread::attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);

    if(!m_core.get_miner().start(adr, static_cast<size_t>(req.threads_count), attrs))
    {
      res.status = "Failed, mining not started";
      return true;
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_stop_mining(const COMMAND_RPC_STOP_MINING::request& req, COMMAND_RPC_STOP_MINING::response& res, connection_context& cntx)
  {
    CHECK_CORE_READY();
    if(!m_core.get_miner().stop())
    {
      res.status = "Failed, mining not stopped";
      return true;
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_getblockcount(const COMMAND_RPC_GETBLOCKCOUNT::request& req, COMMAND_RPC_GETBLOCKCOUNT::response& res, connection_context& cntx)
  {
    CHECK_CORE_READY();
    res.count = m_core.get_current_blockchain_height();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_getblockhash(const COMMAND_RPC_GETBLOCKHASH::request& req, COMMAND_RPC_GETBLOCKHASH::response& res, epee::json_rpc::error& error_resp, connection_context& cntx)
  {
    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy";
      return false;
    }
    if(req.size() != 1)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Wrong parameters, expected height";
      return false;
    }
    uint64_t h = req[0];
    if(m_core.get_current_blockchain_height() <= h)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT;
      error_resp.message = std::string("To big height: ") + std::to_string(h) + ", current blockchain height = " +  std::to_string(m_core.get_current_blockchain_height());
    }
    res = string_tools::pod_to_hex(m_core.get_block_id_by_height(h));
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  uint64_t slow_memmem(void* start_buff, size_t buflen,void* pat,size_t patlen)
  {
    void* buf = start_buff;
    void* end=(char*)buf+buflen-patlen;
    while((buf=memchr(buf,((char*)pat)[0],buflen)))
    {
      if(buf>end)
        return 0;
      if(memcmp(buf,pat,patlen)==0)
        return (char*)buf - (char*)start_buff;
      buf=(char*)buf+1;
    }
    return 0;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_getblocktemplate(const COMMAND_RPC_GETBLOCKTEMPLATE::request& req, COMMAND_RPC_GETBLOCKTEMPLATE::response& res, epee::json_rpc::error& error_resp, connection_context& cntx)
  {
    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy";
      return false;
    }

    if(req.reserve_size > 255)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_RESERVE_SIZE;
      error_resp.message = "To big reserved size, maximum 255";
      return false;
    }

    cryptonote::account_public_address acc = AUTO_VAL_INIT(acc);

    if(!req.wallet_address.size() || !cryptonote::get_account_address_from_str(acc, req.wallet_address))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS;
      error_resp.message = "Failed to parse wallet address";
      return false;
    }

    block b = AUTO_VAL_INIT(b);
    cryptonote::blobdata blob_reserve;
    blob_reserve.resize(req.reserve_size, 0);
    if(!m_core.get_block_template(b, acc, res.difficulty, res.height, blob_reserve))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: failed to create block template";
      LOG_ERROR("Failed to create block template");
      return false;
    }
    blobdata block_blob = t_serializable_object_to_blob(b);
    crypto::public_key tx_pub_key = cryptonote::get_tx_pub_key_from_extra(b.miner_tx);
    if(tx_pub_key == null_pkey)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: failed to create block template";
      LOG_ERROR("Failed to  tx pub key in coinbase extra");
      return false;
    }
    res.reserved_offset = slow_memmem((void*)block_blob.data(), block_blob.size(), &tx_pub_key, sizeof(tx_pub_key));
    if(!res.reserved_offset)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: failed to create block template";
      LOG_ERROR("Failed to find tx pub key in blockblob");
      return false;
    }
    res.reserved_offset += sizeof(tx_pub_key) + 3; //3 bytes: tag for TX_EXTRA_TAG_PUBKEY(1 byte), tag for TX_EXTRA_NONCE(1 byte), counter in TX_EXTRA_NONCE(1 byte)
    if(res.reserved_offset + req.reserve_size > block_blob.size())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: failed to create block template";
      LOG_ERROR("Failed to calculate offset for ");
      return false;
    }
    res.blocktemplate_blob = string_tools::buff_to_hex_nodelimer(block_blob);

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_submitblock(const COMMAND_RPC_SUBMITBLOCK::request& req, COMMAND_RPC_SUBMITBLOCK::response& res, epee::json_rpc::error& error_resp, connection_context& cntx)
  {
    CHECK_CORE_READY();
    if(req.size()!=1)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Wrong param";
      return false;
    }
    blobdata blockblob;
    if(!string_tools::parse_hexstr_to_binbuff(req[0], blockblob))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB;
      error_resp.message = "Wrong block blob";
      return false;
    }
    cryptonote::block_verification_context bvc = AUTO_VAL_INIT(bvc);
    m_core.handle_incoming_block(blockblob, bvc);
    if(!bvc.m_added_to_main_chain)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED;
      error_resp.message = "Block not accepted";
      return false;
    }
    res.status = "OK";
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  uint64_t core_rpc_server::get_block_reward(const block& blk)
  {
    uint64_t reward = 0;
    BOOST_FOREACH(const tx_out& out, blk.miner_tx.vout)
    {
      reward += out.amount;
    }
    return reward;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::get_block_base_reward(const block& blk, uint64_t reward, uint64_t& base_reward)
  {
    uint64_t fee_summ = 0;
    BOOST_FOREACH(const crypto::hash& tx_hash, blk.tx_hashes)
    {
      CHECK_AND_ASSERT_MES(m_core.get_blockchain_storage().have_tx(tx_hash), 0, "Can't find transaction.");
      transaction tx = *m_core.get_blockchain_storage().get_tx(tx_hash);
      if (tx.vin.size() > 0 && tx.vin.front().type() == typeid(txin_gen))
      {
          //It's gen transaction
          continue;
      }
      uint64_t fee;
      CHECK_AND_ASSERT_MES(get_tx_fee(tx, fee), 0, "Can't get fee for transaction.");
      fee_summ += fee;
    }
    base_reward = reward - fee_summ;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::fill_block_header_response(const block& blk, bool orphan_status, uint64_t height, const crypto::hash& hash, block_header_response& response)
  {
    response.major_version = blk.major_version;
    response.minor_version = blk.minor_version;
    response.timestamp = blk.timestamp;
    response.prev_hash = string_tools::pod_to_hex(blk.prev_id);
    response.nonce = blk.nonce;
    response.orphan_status = orphan_status;
    response.height = height;
    response.depth = m_core.get_current_blockchain_height() - height - 1;
    response.hash = string_tools::pod_to_hex(hash);
    response.difficulty = m_core.get_blockchain_storage().block_difficulty(height);
    response.reward = get_block_reward(blk);
    response.tx_count = blk.tx_hashes.size() + 1; //Plus counbase tx.
    std::vector<size_t> sizes;
    bool block_size_status = m_core.get_blockchain_storage().get_backward_blocks_sizes(height, sizes, 1);
    CHECK_AND_ASSERT_MES(block_size_status && 1 == sizes.size(), 0, "Can't get size for block.");
    response.block_size = sizes.front();
    CHECK_AND_ASSERT_MES(get_block_base_reward(blk, response.reward, response.base_reward), 0, "Can't get base reward for block.");
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::get_mixin(const transaction& tx, uint64_t& mixin)
  {
    mixin = 0;
    BOOST_FOREACH(const txin_v& txin, tx.vin)
    {
      CHECK_AND_ASSERT_MES(txin.type() == typeid(txin_to_key), 0, "Unexpected type id in transaction.");
      uint64_t current_mixin = boost::get<txin_to_key>(txin).key_offsets.size();
      if (current_mixin > mixin)
      {
        mixin = current_mixin;
      }
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::fill_tx_out_responce(const tx_out& tx_output, tx_out_response& response)
  {
    response.amount = tx_output.amount;
    if(tx_output.target.type() == typeid(txout_to_key))
    {
      response.tx_out_key = string_tools::pod_to_hex(boost::get<txout_to_key>(tx_output.target).key);
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::fill_tx_in_responce(const txin_to_key& tx_in, tx_in_response& response)
  {
    response.amount = tx_in.amount;
    response.key_offsets = tx_in.key_offsets;
    response.k_image = string_tools::pod_to_hex(tx_in.k_image);
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::fill_tx_response(const transaction& tx, bool in_blockchain, const crypto::hash& block_hash, uint64_t block_height, tx_response& response){
    CHECK_AND_ASSERT_MES(fill_tx_header_response(tx, in_blockchain, block_hash, block_height, response.header), 0, "Can't fill tx_header_response.");
    response.extra = tx.extra;
    response.signatures.reserve(tx.signatures.size());
    BOOST_FOREACH(const std::vector<crypto::signature>& signatures_v, tx.signatures)
    {
      BOOST_FOREACH(const crypto::signature& signature, signatures_v)
      {
          response.signatures.emplace_back(string_tools::pod_to_hex(signature));
      }
    }
    response.inputs.reserve(tx.vin.size());
    BOOST_FOREACH(const txin_v& tx_in, tx.vin)
    {
      if(tx_in.type() == typeid(txin_to_key))
      {
        tx_in_response tx_in_resp;
        CHECK_AND_ASSERT_MES(fill_tx_in_responce(boost::get<txin_to_key>(tx_in), tx_in_resp), 0, "Can't fill tx_in_response.");
        response.inputs.push_back(tx_in_resp);
      }
    }
    response.outputs.reserve(tx.vout.size());
    BOOST_FOREACH(const tx_out& tx_out, tx.vout)
    {
      tx_out_response tx_out_resp;
      CHECK_AND_ASSERT_MES(fill_tx_out_responce(tx_out, tx_out_resp), 0, "Can't fill tx_out_response.");
      response.outputs.push_back(tx_out_resp);
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::fill_tx_header_response(const transaction& tx, bool in_blockchain, const crypto::hash& block_hash, uint64_t block_height, tx_header_response& response)
  {
    response.hash = string_tools::pod_to_hex(get_transaction_hash(tx));
    blobdata blob = t_serializable_object_to_blob(tx);
    response.size = blob.size();
    response.outputs_count = tx.vout.size();
    response.unlock_time = tx.unlock_time;
    response.in_blockchain = in_blockchain;
    response.block_hash = string_tools::pod_to_hex(block_hash);
    response.block_height = block_height;
    response.total_outputs_amount = get_outs_money_amount(tx);
    if (tx.vin.size() > 0 && tx.vin.front().type() == typeid(txin_gen))
    {
      //It's gen transaction
      response.fee = 0;
      response.inputs_count = 0;
      response.total_inputs_amount = 0;
      response.mixin = 0;
      return true;
    }
    response.inputs_count = tx.vin.size();
    uint64_t fee;
    CHECK_AND_ASSERT_MES(get_tx_fee(tx, fee), 0, "Can't get fee for transaction.");
    response.fee = fee;
    uint64_t inputs_amount;
    CHECK_AND_ASSERT_MES(get_inputs_money_amount(tx, inputs_amount), 0, "Can't get inputs_money_amount for transaction.");
    response.total_inputs_amount = inputs_amount;
    uint64_t mixin;
    CHECK_AND_ASSERT_MES(get_mixin(tx, mixin), 0, "Can't get inputs_money_amount for transaction.");
    response.mixin = mixin;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::fill_block_response(const block& blk, bool orphan_status, uint64_t height, const crypto::hash& hash, block_response& response)
  {
    CHECK_AND_ASSERT_MES(fill_block_header_response(blk, orphan_status, height, hash, response.block_header), 0, "Can't fill block_header_response.");
    response.txs.reserve(blk.tx_hashes.size() + 1); //Plus miner_tx
    tx_header_response tx_header;
    CHECK_AND_ASSERT_MES(fill_tx_header_response(blk.miner_tx, true, hash, height, tx_header), 0, "Can't fill tx_header_response.");
    response.txs.push_back(tx_header);
    BOOST_FOREACH(const crypto::hash& tx_hash, blk.tx_hashes)
    {
      CHECK_AND_ASSERT_MES(m_core.get_blockchain_storage().have_tx(tx_hash), 0, "Can't find transaction.");
      tx_header_response tx_info;
      CHECK_AND_ASSERT_MES(fill_tx_header_response(*m_core.get_blockchain_storage().get_tx(tx_hash), true, hash, height, tx_info), 0, "Can't fill tx_info_response.");
      response.txs.push_back(tx_info);
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::get_last_block(block_response& blk_resp, std::string& error_description)
  {
    uint64_t last_block_height;
    crypto::hash last_block_hash;
    bool have_last_block_hash = m_core.get_blockchain_top(last_block_height, last_block_hash);
    if (!have_last_block_hash)
    {
      error_description = "Internal error: can't get last block hash.";
      return false;
    }
    return get_block_by_hash(last_block_hash, blk_resp, error_description);
  }
  
  bool core_rpc_server::get_block_by_height(uint64_t block_height, block_response& blk_resp, std::string& error_description)
  {
      crypto::hash block_hash = m_core.get_block_id_by_height(block_height);
      if (null_hash == block_hash) {
          error_description = "Internal error: can't get block by height. Height = " + std::to_string(block_height) + '.';
          return false;
      }
      return get_block_by_hash(block_hash, blk_resp, error_description);
  }
  
  bool core_rpc_server::get_block_by_hash(const crypto::hash& block_hash, block_response& blk_resp, std::string& error_description)
  {
    block blk;
    bool have_block = m_core.get_block_by_hash(block_hash, blk);
    if (!have_block)
    {
      error_description = "Internal error: can't get block by hash. Hash = " + string_tools::pod_to_hex(block_hash) + '.';
      return false;
    }
    if (blk.miner_tx.vin.front().type() != typeid(txin_gen))
    {
      error_description = "Internal error: coinbase transaction in the block has the wrong type";
      return false;
    }
    uint64_t block_height = boost::get<txin_gen>(blk.miner_tx.vin.front()).height;
    bool responce_filled = fill_block_response(blk, false, block_height, block_hash, blk_resp);
    if (!responce_filled)
    {
      error_description = "Internal error: can't produce valid response.";
      return false;
    }
    
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::get_tx_by_hash(const crypto::hash& tx_hash, tx_response& tx_resp, std::string& error_description)
  {
    transaction tx;
    bool have_tx = m_core.get_transaction(tx_hash, tx);
    if (!have_tx)
    {
      error_description = "Internal error: can't get tx by hash. Hash = " + string_tools::pod_to_hex(tx_hash) + '.';
      return false;
    }
    crypto::hash block_hash;
    uint64_t block_height;
    bool in_blockchain = m_core.get_blockchain_storage().get_block_containing_tx(tx_hash, block_hash, block_height);
    bool responce_filled = fill_tx_response(tx, in_blockchain, block_hash, block_height, tx_resp);
    if (!responce_filled)
    {
      error_description = "Internal error: can't produce valid response.";
      return false;
    }
    
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::get_tx_header_by_hash(const crypto::hash& tx_hash, tx_header_response& tx_resp, std::string& error_description)
  {
    transaction tx;
    bool have_tx = m_core.get_transaction(tx_hash, tx);
    if (!have_tx)
    {
      error_description = "Internal error: can't get tx by hash. Hash = " + string_tools::pod_to_hex(tx_hash) + '.';
      return false;
    }
    crypto::hash block_hash;
    uint64_t block_height;
    bool in_blockchain = m_core.get_blockchain_storage().get_block_containing_tx(tx_hash, block_hash, block_height);
    bool responce_filled = fill_tx_header_response(tx, in_blockchain, block_hash, block_height, tx_resp);
    if (!responce_filled)
    {
      error_description = "Internal error: can't produce valid response.";
      return false;
    }
    
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_last_block_header(const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request& req, COMMAND_RPC_GET_LAST_BLOCK_HEADER::response& res, epee::json_rpc::error& error_resp, connection_context& cntx)
  {
    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy.";
      return false;
    }
    
    block_response blk;
    std::string error_description;
    bool got_block = get_last_block(blk, error_description);
    
    if (!got_block)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = error_description;
      return false;
    }
    
    res.block_header = blk.block_header;
    
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_block_header_by_hash(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response& res, epee::json_rpc::error& error_resp, connection_context& cntx){
    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy.";
      return false;
    }
    
    crypto::hash block_hash;
    bool hash_parsed = parse_hash256(req.hash, block_hash);
    if(!hash_parsed)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Failed to parse hex representation of block hash. Hex = " + req.hash + '.';
      return false;
    }

    block_response blk;
    std::string error_description;
    bool got_block = get_block_by_hash(block_hash, blk, error_description);
    
    if (!got_block)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = error_description;
      return false;
    }
    
    res.block_header = blk.block_header;
    
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_block_header_by_height(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response& res, epee::json_rpc::error& error_resp, connection_context& cntx){
    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy.";
      return false;
    }
    
    if(m_core.get_current_blockchain_height() <= req.height)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT;
      error_resp.message = std::string("Too big height: ") + std::to_string(req.height) + ", current blockchain height = " +  std::to_string(m_core.get_current_blockchain_height());
      return false;
    }
    
    block_response blk;
    std::string error_description;
    bool got_block = get_block_by_height(req.height, blk, error_description);
    
    if (!got_block)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = error_description;
      return false;
    }
    
    res.block_header = blk.block_header;
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------

  bool core_rpc_server::on_get_last_block(const COMMAND_RPC_GET_LAST_BLOCK::request& req, COMMAND_RPC_GET_LAST_BLOCK::response& res, epee::json_rpc::error& error_resp, connection_context& cntx)
  {
    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy.";
      return false;
    }
    
    std::string error_description;
    bool got_block = get_last_block(res.block, error_description);
    
    if (!got_block)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = error_description;
      return false;
    }
    
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_block_by_hash(const COMMAND_RPC_GET_BLOCK_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_BY_HASH::response& res, epee::json_rpc::error& error_resp, connection_context& cntx){
    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy.";
      return false;
    }
    
    crypto::hash block_hash;
    bool hash_parsed = parse_hash256(req.hash, block_hash);
    if(!hash_parsed)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Failed to parse hex representation of block hash. Hex = " + req.hash + '.';
      return false;
    }

    std::string error_description;
    bool got_block = get_block_by_hash(block_hash, res.block, error_description);
    
    if (!got_block)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = error_description;
      return false;
    }
    
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_block_by_height(const COMMAND_RPC_GET_BLOCK_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_BY_HEIGHT::response& res, epee::json_rpc::error& error_resp, connection_context& cntx){
    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy.";
      return false;
    }
    
    if(m_core.get_current_blockchain_height() <= req.height)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT;
      error_resp.message = std::string("To big height: ") + std::to_string(req.height) + ", current blockchain height = " +  std::to_string(m_core.get_current_blockchain_height());
      return false;
    }
    
    std::string error_description;
    bool got_block = get_block_by_height(req.height, res.block, error_description);
    
    if (!got_block)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = error_description;
      return false;
    }
    
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_tx_by_hash(const COMMAND_RPC_GET_TX_BY_HASH::request& req, COMMAND_RPC_GET_TX_BY_HASH::response& res, epee::json_rpc::error& error_resp, connection_context& cntx)
  {
    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy.";
      return false;
    }
    
    crypto::hash tx_hash;
    bool hash_parsed = parse_hash256(req.hash, tx_hash);
    if(!hash_parsed)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Failed to parse hex representation of tx hash. Hex = " + req.hash + '.';
      return false;
    }

    std::string error_description;
    bool got_tx = get_tx_by_hash(tx_hash, res.tx, error_description);
    
    if (!got_tx)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = error_description;
      return false;
    }
    
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_tx_header_by_hash(const COMMAND_RPC_GET_TX_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_TX_HEADER_BY_HASH::response& res, epee::json_rpc::error& error_resp, connection_context& cntx){
    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy.";
      return false;
    }
    
    crypto::hash tx_hash;
    bool hash_parsed = parse_hash256(req.hash, tx_hash);
    if(!hash_parsed)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Failed to parse hex representation of tx hash. Hex = " + req.hash + '.';
      return false;
    }

    std::string error_description;
    bool got_tx = get_tx_header_by_hash(tx_hash, res.tx_header, error_description);
    
    if (!got_tx)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = error_description;
      return false;
    }
    
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
}
