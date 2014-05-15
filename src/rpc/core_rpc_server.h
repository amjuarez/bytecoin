// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma  once 

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

#include "net/http_server_impl_base.h"
#include "core_rpc_server_commands_defs.h"
#include "cryptonote_core/cryptonote_core.h"
#include "p2p/net_node.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"

namespace cryptonote
{
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class core_rpc_server: public epee::http_server_impl_base<core_rpc_server>
  {
  public:
    typedef epee::net_utils::connection_context_base connection_context;

    core_rpc_server(core& cr, nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core> >& p2p);

    static void init_options(boost::program_options::options_description& desc);
    bool init(const boost::program_options::variables_map& vm);
  private:

    CHAIN_HTTP_TO_MAP2(connection_context); //forward http requests to uri map

    BEGIN_URI_MAP2()
      MAP_URI_AUTO_JON2("/getheight", on_get_height, COMMAND_RPC_GET_HEIGHT)
      MAP_URI_AUTO_BIN2("/getblocks.bin", on_get_blocks, COMMAND_RPC_GET_BLOCKS_FAST)
      MAP_URI_AUTO_BIN2("/get_o_indexes.bin", on_get_indexes, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES)      
      MAP_URI_AUTO_BIN2("/getrandom_outs.bin", on_get_random_outs, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS)      
      MAP_URI_AUTO_JON2("/gettransactions", on_get_transactions, COMMAND_RPC_GET_TRANSACTIONS)
      MAP_URI_AUTO_JON2("/sendrawtransaction", on_send_raw_tx, COMMAND_RPC_SEND_RAW_TX)
      MAP_URI_AUTO_JON2("/start_mining", on_start_mining, COMMAND_RPC_START_MINING)
      MAP_URI_AUTO_JON2("/stop_mining", on_stop_mining, COMMAND_RPC_STOP_MINING)
      MAP_URI_AUTO_JON2("/getinfo", on_get_info, COMMAND_RPC_GET_INFO)
      BEGIN_JSON_RPC_MAP("/json_rpc")
        MAP_JON_RPC("getblockcount",             on_getblockcount,              COMMAND_RPC_GETBLOCKCOUNT)
        MAP_JON_RPC_WE("on_getblockhash",        on_getblockhash,               COMMAND_RPC_GETBLOCKHASH)
        MAP_JON_RPC_WE("getblocktemplate",       on_getblocktemplate,           COMMAND_RPC_GETBLOCKTEMPLATE)
        MAP_JON_RPC_WE("submitblock",            on_submitblock,                COMMAND_RPC_SUBMITBLOCK)
        MAP_JON_RPC_WE("getlastblockheader",     on_get_last_block_header,      COMMAND_RPC_GET_LAST_BLOCK_HEADER)
        MAP_JON_RPC_WE("getblockheaderbyhash",   on_get_block_header_by_hash,   COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH)
        MAP_JON_RPC_WE("getblockheaderbyheight", on_get_block_header_by_height, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT)
        MAP_JON_RPC_WE("getlastblock",           on_get_last_block,             COMMAND_RPC_GET_LAST_BLOCK)
        MAP_JON_RPC_WE("getblockbyhash",         on_get_block_by_hash,          COMMAND_RPC_GET_BLOCK_BY_HASH)
        MAP_JON_RPC_WE("getblockbyheight",       on_get_block_by_height,        COMMAND_RPC_GET_BLOCK_BY_HEIGHT)
        MAP_JON_RPC_WE("gettxbyhash",            on_get_tx_by_hash,             COMMAND_RPC_GET_TX_BY_HASH)
        MAP_JON_RPC_WE("gettxheaderbyhash",      on_get_tx_header_by_hash,      COMMAND_RPC_GET_TX_HEADER_BY_HASH)
      END_JSON_RPC_MAP()
    END_URI_MAP2()

    bool on_get_height(const COMMAND_RPC_GET_HEIGHT::request& req, COMMAND_RPC_GET_HEIGHT::response& res, connection_context& cntx);
    bool on_get_blocks(const COMMAND_RPC_GET_BLOCKS_FAST::request& req, COMMAND_RPC_GET_BLOCKS_FAST::response& res, connection_context& cntx);
    bool on_get_transactions(const COMMAND_RPC_GET_TRANSACTIONS::request& req, COMMAND_RPC_GET_TRANSACTIONS::response& res, connection_context& cntx);
    bool on_get_indexes(const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request& req, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response& res, connection_context& cntx);
    bool on_send_raw_tx(const COMMAND_RPC_SEND_RAW_TX::request& req, COMMAND_RPC_SEND_RAW_TX::response& res, connection_context& cntx);
    bool on_start_mining(const COMMAND_RPC_START_MINING::request& req, COMMAND_RPC_START_MINING::response& res, connection_context& cntx);
    bool on_stop_mining(const COMMAND_RPC_STOP_MINING::request& req, COMMAND_RPC_STOP_MINING::response& res, connection_context& cntx);
    bool on_get_random_outs(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res, connection_context& cntx);        
    bool on_get_info(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res, connection_context& cntx);        
    
    //json_rpc
    bool on_getblockcount(const COMMAND_RPC_GETBLOCKCOUNT::request& req, COMMAND_RPC_GETBLOCKCOUNT::response& res, connection_context& cntx);
    bool on_getblockhash(const COMMAND_RPC_GETBLOCKHASH::request& req, COMMAND_RPC_GETBLOCKHASH::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    bool on_getblocktemplate(const COMMAND_RPC_GETBLOCKTEMPLATE::request& req, COMMAND_RPC_GETBLOCKTEMPLATE::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    bool on_submitblock(const COMMAND_RPC_SUBMITBLOCK::request& req, COMMAND_RPC_SUBMITBLOCK::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    
    bool on_get_last_block_header(const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request& req, COMMAND_RPC_GET_LAST_BLOCK_HEADER::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    bool on_get_block_header_by_hash(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    bool on_get_block_header_by_height(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    
    bool on_get_last_block(const COMMAND_RPC_GET_LAST_BLOCK::request& req, COMMAND_RPC_GET_LAST_BLOCK::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    bool on_get_block_by_hash(const COMMAND_RPC_GET_BLOCK_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_BY_HASH::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    bool on_get_block_by_height(const COMMAND_RPC_GET_BLOCK_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_BY_HEIGHT::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    
    bool on_get_tx_by_hash(const COMMAND_RPC_GET_TX_BY_HASH::request& req, COMMAND_RPC_GET_TX_BY_HASH::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    bool on_get_tx_header_by_hash(const COMMAND_RPC_GET_TX_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_TX_HEADER_BY_HASH::response& res, epee::json_rpc::error& error_resp, connection_context& cntx);
    //-----------------------
    bool handle_command_line(const boost::program_options::variables_map& vm);
    bool check_core_ready();
    
    //utils
    uint64_t get_block_reward(const block& blk);
    bool get_block_base_reward(const block& blk, uint64_t reward, uint64_t& base_reward);
    bool fill_block_header_response(const block& blk, bool orphan_status, uint64_t height, const crypto::hash& hash, block_header_response& response);
    bool fill_block_response(const block& blk, bool orphan_status, uint64_t height, const crypto::hash& hash, block_response& response);
    bool fill_tx_header_response(const transaction& tx, bool in_blockchain, const crypto::hash& block_hash, uint64_t block_height, tx_header_response& response);
    bool fill_tx_response(const transaction& tx, bool in_blockchain, const crypto::hash& block_hash, uint64_t block_height, tx_response& response);
    bool fill_tx_out_responce(const tx_out& tx_output, tx_out_response& response);
    bool fill_tx_in_responce(const txin_to_key& tx_in, tx_in_response& response);
    bool get_block_by_hash(const crypto::hash& block_hash, block_response& blk, std::string& error_description);
    bool get_block_by_height(uint64_t block_height, block_response& blk, std::string& error_description);
    bool get_last_block(block_response& blk, std::string& error_description);
    bool get_mixin(const transaction& tx, uint64_t& mixin);
    bool get_tx_by_hash(const crypto::hash& tx_hash, tx_response& tx_resp, std::string& error_description);
    bool get_tx_header_by_hash(const crypto::hash& tx_hash, tx_header_response& tx_resp, std::string& error_description);
    
    core& m_core;
    nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core> >& m_p2p;
    std::string m_port;
    std::string m_bind_ip;
  };
}
