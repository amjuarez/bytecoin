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

#pragma  once

#include <future>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include "net/http_server_impl_base.h"
#include "wallet_rpc_server_commans_defs.h"
#include "Wallet.h"
#include "common/command_line.h"
namespace tools
{
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class wallet_rpc_server: public epee::http_server_impl_base<wallet_rpc_server>, public CryptoNote::IWalletObserver
  {
  public:
    typedef epee::net_utils::connection_context_base connection_context;

    wallet_rpc_server(CryptoNote::IWallet &w, CryptoNote::INode &n, cryptonote::Currency& currency, const std::string& walletFilename);

    const static command_line::arg_descriptor<std::string> arg_rpc_bind_port;
    const static command_line::arg_descriptor<std::string> arg_rpc_bind_ip;

    //---------------- IWalletObserver -------------------------
    virtual void saveCompleted(std::error_code result) override;
    //----------------------------------------------------------

    static void init_options(boost::program_options::options_description& desc);
    bool init(const boost::program_options::variables_map& vm);
    bool run();
  private:

    CHAIN_HTTP_TO_MAP2(connection_context); //forward http requests to uri map

    BEGIN_URI_MAP2()
      BEGIN_JSON_RPC_MAP("/json_rpc")
        MAP_JON_RPC_WE("getbalance",    on_getbalance,    wallet_rpc::COMMAND_RPC_GET_BALANCE)
        MAP_JON_RPC_WE("transfer",      on_transfer,      wallet_rpc::COMMAND_RPC_TRANSFER)
        MAP_JON_RPC_WE("store",         on_store,         wallet_rpc::COMMAND_RPC_STORE)
        MAP_JON_RPC_WE("get_payments",  on_get_payments,  wallet_rpc::COMMAND_RPC_GET_PAYMENTS)
        MAP_JON_RPC_WE("get_transfers", on_get_transfers, wallet_rpc::COMMAND_RPC_GET_TRANSFERS)
        MAP_JON_RPC_WE("get_height", on_get_height, wallet_rpc::COMMAND_RPC_GET_HEIGHT)
        MAP_JON_RPC_WE("reset", on_reset, wallet_rpc::COMMAND_RPC_RESET)
      END_JSON_RPC_MAP()
    END_URI_MAP2()

      //json_rpc
      bool on_getbalance(const wallet_rpc::COMMAND_RPC_GET_BALANCE::request& req, wallet_rpc::COMMAND_RPC_GET_BALANCE::response& res, epee::json_rpc::error& er, connection_context& cntx);
      bool on_transfer(const wallet_rpc::COMMAND_RPC_TRANSFER::request& req, wallet_rpc::COMMAND_RPC_TRANSFER::response& res, epee::json_rpc::error& er, connection_context& cntx);
      bool on_store(const wallet_rpc::COMMAND_RPC_STORE::request& req, wallet_rpc::COMMAND_RPC_STORE::response& res, epee::json_rpc::error& er, connection_context& cntx);
      bool on_get_payments(const wallet_rpc::COMMAND_RPC_GET_PAYMENTS::request& req, wallet_rpc::COMMAND_RPC_GET_PAYMENTS::response& res, epee::json_rpc::error& er, connection_context& cntx);
      bool on_get_transfers(const wallet_rpc::COMMAND_RPC_GET_TRANSFERS::request& req, wallet_rpc::COMMAND_RPC_GET_TRANSFERS::response& res, epee::json_rpc::error& er, connection_context& cntx);
      bool on_get_height(const wallet_rpc::COMMAND_RPC_GET_HEIGHT::request& req, wallet_rpc::COMMAND_RPC_GET_HEIGHT::response& res, epee::json_rpc::error& er, connection_context& cntx);
      bool on_reset(const wallet_rpc::COMMAND_RPC_RESET::request& req, wallet_rpc::COMMAND_RPC_RESET::response& res, epee::json_rpc::error& er, connection_context& cntx);

      bool handle_command_line(const boost::program_options::variables_map& vm);

      CryptoNote::IWallet& m_wallet;
      CryptoNote::INode& m_node;
      std::string m_port;
      std::string m_bind_ip;
      cryptonote::Currency& m_currency;
      const std::string m_walletFilename;

      std::unique_ptr<std::promise<std::error_code>> m_saveResultPromise;
  };
}
