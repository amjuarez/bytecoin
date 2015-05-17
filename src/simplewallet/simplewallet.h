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

#include <memory>
#include <future>

#include <boost/program_options/variables_map.hpp>

#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/Currency.h"
#include "console_handler.h"
#include "password_container.h"
#include "IWallet.h"
#include "INode.h"
#include "wallet/WalletHelper.h"
#include "net/http_client.h"

namespace cryptonote
{
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class simple_wallet : public CryptoNote::INodeObserver, public CryptoNote::IWalletObserver
  {
  public:
    typedef std::vector<std::string> command_type;

    simple_wallet(const cryptonote::Currency& currency);
    bool init(const boost::program_options::variables_map& vm);
    bool deinit();
    bool run();
    void stop();

    //wallet *create_wallet();
    bool process_command(const std::vector<std::string> &args);
    std::string get_commands_str();

    const cryptonote::Currency& currency() const { return m_currency; }

  private:
    void handle_command_line(const boost::program_options::variables_map& vm);

    bool run_console_handler();

    bool new_wallet(const std::string &wallet_file, const std::string& password);
    bool open_wallet(const std::string &wallet_file, const std::string& password);
    bool close_wallet();

    bool help(const std::vector<std::string> &args = std::vector<std::string>());
    bool start_mining(const std::vector<std::string> &args);
    bool stop_mining(const std::vector<std::string> &args);
    //bool refresh(const std::vector<std::string> &args = std::vector<std::string>());
    bool show_balance(const std::vector<std::string> &args = std::vector<std::string>());
    bool show_incoming_transfers(const std::vector<std::string> &args);
    bool show_payments(const std::vector<std::string> &args);
    bool show_blockchain_height(const std::vector<std::string> &args);
    bool listTransfers(const std::vector<std::string> &args);
    bool transfer(const std::vector<std::string> &args);
    bool print_address(const std::vector<std::string> &args = std::vector<std::string>());
    bool save(const std::vector<std::string> &args);
    bool reset(const std::vector<std::string> &args);
    bool set_log(const std::vector<std::string> &args);

    //uint64_t get_daemon_blockchain_height(std::string& err);
    //bool try_connect_to_daemon();
    bool ask_wallet_create_if_needed();

    ////----------------- i_wallet2_callback ---------------------
    //virtual void on_money_received(uint64_t height, const cryptonote::Transaction& tx, size_t out_index);
    //virtual void on_money_spent(uint64_t height, const cryptonote::Transaction& in_tx, size_t out_index, const cryptonote::Transaction& spend_tx);
    //virtual void on_skip_transaction(uint64_t height, const cryptonote::Transaction& tx);
    ////----------------------------------------------------------

    //---------------- IWalletObserver -------------------------
    virtual void initCompleted(std::error_code result) override;
    virtual void saveCompleted(std::error_code result) override;
    virtual void externalTransactionCreated(CryptoNote::TransactionId transactionId) override;
    //----------------------------------------------------------

    //----------------- INodeObserver --------------------------
    virtual void localBlockchainUpdated(uint64_t height) override;
    //----------------------------------------------------------

    friend class refresh_progress_reporter_t;

    class refresh_progress_reporter_t
    {
    public:
      refresh_progress_reporter_t(cryptonote::simple_wallet& simple_wallet)
        : m_simple_wallet(simple_wallet)
        , m_blockchain_height(0)
        , m_blockchain_height_update_time()
        , m_print_time()
      {
      }

      void update(uint64_t height, bool force = false)
      {
        auto current_time = std::chrono::system_clock::now();
        if (std::chrono::seconds(m_simple_wallet.currency().difficultyTarget() / 2) < current_time - m_blockchain_height_update_time ||
            m_blockchain_height <= height) {
          update_blockchain_height();
          m_blockchain_height = (std::max)(m_blockchain_height, height);
        }

        if (std::chrono::milliseconds(1) < current_time - m_print_time || force)
        {
          LOG_PRINT_L0("Height " << height << " of " << m_blockchain_height << '\r');
          m_print_time = current_time;
        }
      }

    private:
      void update_blockchain_height()
      {
        std::string err;
        uint64_t blockchain_height = m_simple_wallet.m_node->getLastLocalBlockHeight();
        if (err.empty())
        {
          m_blockchain_height = blockchain_height;
          m_blockchain_height_update_time = std::chrono::system_clock::now();
        }
        else
        {
          LOG_ERROR("Failed to get current blockchain height: " << err);
        }
      }

    private:
      cryptonote::simple_wallet& m_simple_wallet;
      uint64_t m_blockchain_height;
      std::chrono::system_clock::time_point m_blockchain_height_update_time;
      std::chrono::system_clock::time_point m_print_time;
    };

  private:
    std::string m_wallet_file_arg;
    std::string m_generate_new;
    std::string m_import_path;

    std::string m_daemon_address;
    std::string m_daemon_host;
    int m_daemon_port;

    std::string m_wallet_file;

    std::unique_ptr<std::promise<std::error_code>> m_initResultPromise;
    std::unique_ptr<std::promise<std::error_code>> m_saveResultPromise;

    epee::console_handlers_binder m_cmd_binder;

    const cryptonote::Currency& m_currency;

    std::unique_ptr<CryptoNote::INode> m_node;
    std::unique_ptr<CryptoNote::IWallet> m_wallet;
    epee::net_utils::http::http_simple_client m_http_client;
    refresh_progress_reporter_t m_refresh_progress_reporter;
  };
}
