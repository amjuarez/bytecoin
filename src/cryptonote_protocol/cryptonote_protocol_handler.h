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

#pragma once

#include <atomic>

#include <boost/program_options/variables_map.hpp>
#include <Common/ObserverManager.h>

#include "cryptonote_core/ICore.h"

#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_protocol/cryptonote_protocol_handler_common.h"
#include "cryptonote_protocol/ICryptonoteProtocolObserver.h"
#include "cryptonote_protocol/ICryptonoteProtocolQuery.h"

#include "p2p/p2p_protocol_defs.h"
#include "p2p/net_node_common.h"
#include "p2p/connection_context.h"

#include <Logging/LoggerRef.h>

PUSH_WARNINGS
DISABLE_VS_WARNINGS(4355)

namespace System {
  class Dispatcher;
}

namespace CryptoNote
{
  class Currency;

  class cryptonote_protocol_handler : 
    public i_cryptonote_protocol, 
    public ICryptonoteProtocolQuery
  {
  public:

    cryptonote_protocol_handler(const Currency& currency, System::Dispatcher& dispatcher, ICore& rcore, i_p2p_endpoint* p_net_layout, Logging::ILogger& log);

    virtual bool addObserver(ICryptonoteProtocolObserver* observer);
    virtual bool removeObserver(ICryptonoteProtocolObserver* observer);

    void set_p2p_endpoint(i_p2p_endpoint* p2p);
    // ICore& get_core() { return m_core; }
    bool is_synchronized() const { return m_synchronized; }
    void log_connections();

    // Interface t_payload_net_handler, where t_payload_net_handler is template argument of nodetool::node_server
    void stop();
    bool start_sync(cryptonote_connection_context& context);
    bool on_idle();
    void onConnectionOpened(cryptonote_connection_context& context);
    void onConnectionClosed(cryptonote_connection_context& context);
    bool get_stat_info(core_stat_info& stat_inf);
    bool get_payload_sync_data(CORE_SYNC_DATA& hshd);
    bool process_payload_sync_data(const CORE_SYNC_DATA& hshd, cryptonote_connection_context& context, bool is_inital);
    int handleCommand(bool is_notify, int command, const std::string& in_buff, std::string& buff_out, cryptonote_connection_context& context, bool& handled);
    virtual size_t getPeerCount() const;
    virtual uint64_t getObservedHeight() const;

  private:
    //----------------- commands handlers ----------------------------------------------
    int handle_notify_new_block(int command, NOTIFY_NEW_BLOCK::request& arg, cryptonote_connection_context& context);
    int handle_notify_new_transactions(int command, NOTIFY_NEW_TRANSACTIONS::request& arg, cryptonote_connection_context& context);
    int handle_request_get_objects(int command, NOTIFY_REQUEST_GET_OBJECTS::request& arg, cryptonote_connection_context& context);
    int handle_response_get_objects(int command, NOTIFY_RESPONSE_GET_OBJECTS::request& arg, cryptonote_connection_context& context);
    int handle_request_chain(int command, NOTIFY_REQUEST_CHAIN::request& arg, cryptonote_connection_context& context);
    int handle_response_chain_entry(int command, NOTIFY_RESPONSE_CHAIN_ENTRY::request& arg, cryptonote_connection_context& context);

    //----------------- i_cryptonote_protocol ----------------------------------
    virtual void relay_block(NOTIFY_NEW_BLOCK::request& arg) override;
    virtual void relay_transactions(NOTIFY_NEW_TRANSACTIONS::request& arg) override;

    //----------------------------------------------------------------------------------
    uint64_t get_current_blockchain_height();
    bool request_missing_objects(cryptonote_connection_context& context, bool check_having_blocks);
    bool on_connection_synchronized();
    void updateObservedHeight(uint64_t peerHeight, const cryptonote_connection_context& context);
    void recalculateMaxObservedHeight(const cryptonote_connection_context& context);
    int processObjects(cryptonote_connection_context& context, const std::list<block_complete_entry>& blocks);
    Logging::LoggerRef logger;

  private:

    System::Dispatcher& m_dispatcher;
    ICore& m_core;
    const Currency& m_currency;

    p2p_endpoint_stub m_p2p_stub;
    i_p2p_endpoint* m_p2p;
    std::atomic<bool> m_synchronized;
    std::atomic<bool> m_stop;

    mutable std::mutex m_observedHeightMutex;
    uint64_t m_observedHeight;

    std::atomic<size_t> m_peersCount;
    tools::ObserverManager<ICryptonoteProtocolObserver> m_observerManager;
  };
}

POP_WARNINGS
