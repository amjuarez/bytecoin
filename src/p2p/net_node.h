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

#include <functional>
#include <unordered_map>

#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/serialization/version.hpp>

#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/Latch.h>
#include <System/Timer.h>
#include <System/TcpConnection.h>
#include <System/TcpListener.h>

#include "cryptonote_core/OnceInInterval.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "Common/command_line.h"
#include "Logging/LoggerRef.h"

#include "connection_context.h"
#include "LevinProtocol.h"
#include "net_node_common.h"
#include "NetNodeConfig.h"
#include "p2p_protocol_defs.h"
#include "p2p_networks.h"
#include "PeerListManager.h"

namespace System {
class TcpConnection;
}

namespace CryptoNote
{
  class LevinProtocol;

  std::string print_peerlist_to_string(const std::list<peerlist_entry>& pl);

  struct p2p_connection_context : public cryptonote_connection_context {

    p2p_connection_context(System::Dispatcher& dispatcher, System::TcpConnection&& conn) : 
      peer_id(0), connectionEvent(dispatcher), writeLatch(dispatcher), connection(std::move(conn)) {
      connectionEvent.set();
    }

    p2p_connection_context(p2p_connection_context&& ctx) : 
      cryptonote_connection_context(std::move(ctx)) {
      connection = std::move(ctx.connection);
      connectionEvent = std::move(ctx.connectionEvent);
      writeLatch = std::move(ctx.writeLatch);
      peer_id = ctx.peer_id;
    }

    peerid_type peer_id;
    System::TcpConnection connection;
    System::Event connectionEvent;
    System::Latch writeLatch;
  };

  class node_server :  public i_p2p_endpoint
  {
  public:

    static void init_options(boost::program_options::options_description& desc);

    node_server(System::Dispatcher& dispatcher, CryptoNote::cryptonote_protocol_handler& payload_handler, Logging::ILogger& log);

    bool run();
    bool init(const NetNodeConfig& config, bool testnet);
    bool deinit();
    bool send_stop_signal();
    uint32_t get_this_peer_port(){return m_listeningPort;}
    CryptoNote::cryptonote_protocol_handler& get_payload_object();

    template <class Archive, class t_version_type>
    void serialize(Archive &a,  const t_version_type ver) {
      a & m_peerlist;
      a & m_config.m_peer_id;
    }

    // debug functions
    bool log_peerlist();
    bool log_connections();
    virtual uint64_t get_connections_count();
    size_t get_outgoing_connections_count();

    CryptoNote::peerlist_manager& get_peerlist_manager() { return m_peerlist; }

  private:

    int handleCommand(const LevinProtocol::Command& cmd, std::string& buff_out, p2p_connection_context& context, bool& handled);

    //----------------- commands handlers ----------------------------------------------
    int handle_handshake(int command, COMMAND_HANDSHAKE::request& arg, COMMAND_HANDSHAKE::response& rsp, p2p_connection_context& context);
    int handle_timed_sync(int command, COMMAND_TIMED_SYNC::request& arg, COMMAND_TIMED_SYNC::response& rsp, p2p_connection_context& context);
    int handle_ping(int command, COMMAND_PING::request& arg, COMMAND_PING::response& rsp, p2p_connection_context& context);
#ifdef ALLOW_DEBUG_COMMANDS
    int handle_get_stat_info(int command, COMMAND_REQUEST_STAT_INFO::request& arg, COMMAND_REQUEST_STAT_INFO::response& rsp, p2p_connection_context& context);
    int handle_get_network_state(int command, COMMAND_REQUEST_NETWORK_STATE::request& arg, COMMAND_REQUEST_NETWORK_STATE::response& rsp, p2p_connection_context& context);
    int handle_get_peer_id(int command, COMMAND_REQUEST_PEER_ID::request& arg, COMMAND_REQUEST_PEER_ID::response& rsp, p2p_connection_context& context);
#endif

    bool init_config();
    bool make_default_config();
    bool store_config();
    bool check_trust(const proof_of_trust& tr);
    void initUpnp();

    bool handshake(CryptoNote::LevinProtocol& proto, p2p_connection_context& context, bool just_take_peerlist = false);
    bool timedSync();
    bool handleTimedSyncResponse(const std::string& in, p2p_connection_context& context);
    void forEachConnection(std::function<void(p2p_connection_context&)> action);

    void on_connection_new(p2p_connection_context& context);
    void on_connection_close(p2p_connection_context& context);

    //----------------- i_p2p_endpoint -------------------------------------------------------------
    virtual void relay_notify_to_all(int command, const std::string& data_buff, const net_connection_id* excludeConnection) override;
    virtual bool invoke_notify_to_peer(int command, const std::string& req_buff, const cryptonote_connection_context& context) override;
    virtual void for_each_connection(std::function<void(CryptoNote::cryptonote_connection_context&, peerid_type)> f) override;
    virtual void externalRelayNotifyToAll(int command, const std::string& data_buff) override;

    //-----------------------------------------------------------------------------------------------
    bool handle_command_line(const boost::program_options::variables_map& vm);
    bool handleConfig(const NetNodeConfig& config);
    bool append_net_address(std::vector<net_address>& nodes, const std::string& addr);
    bool idle_worker();
    bool handle_remote_peerlist(const std::list<peerlist_entry>& peerlist, time_t local_time, const cryptonote_connection_context& context);
    bool get_local_node_data(basic_node_data& node_data);

    bool merge_peerlist_with_local(const std::list<peerlist_entry>& bs);
    bool fix_time_delta(std::list<peerlist_entry>& local_peerlist, time_t local_time, int64_t& delta);

    bool connections_maker();
    bool make_new_connection_from_peerlist(bool use_white_list);
    bool try_to_connect_and_handshake_with_new_peer(const net_address& na, bool just_take_peerlist = false, uint64_t last_seen_stamp = 0, bool white = true);
    bool is_peer_used(const peerlist_entry& peer);
    bool is_addr_connected(const net_address& peer);  
    bool try_ping(basic_node_data& node_data, p2p_connection_context& context);
    bool make_expected_connections_count(bool white_list, size_t expected_connections);
    bool is_priority_node(const net_address& na);

    bool connect_to_peerlist(const std::vector<net_address>& peers);

    bool parse_peers_and_add_to_container(const boost::program_options::variables_map& vm, 
      const command_line::arg_descriptor<std::vector<std::string> > & arg, std::vector<net_address>& container);

    //debug functions
    std::string print_connections_container();

    typedef std::unordered_map<boost::uuids::uuid, p2p_connection_context, boost::hash<boost::uuids::uuid>> ConnectionContainer;
    typedef ConnectionContainer::iterator ConnectionIterator;
    ConnectionContainer m_raw_connections;
    ConnectionContainer m_connections;

    void acceptLoop();
    void connectionHandler(const boost::uuids::uuid& connectionId, p2p_connection_context& connection);
    void onIdle();
    void timedSyncLoop();

    struct config
    {
      network_config m_net_config;
      uint64_t m_peer_id;

      void serialize(ISerializer& s) {
        KV_MEMBER(m_net_config)
        KV_MEMBER(m_peer_id)
      }
    };

    config m_config;
    std::string m_config_folder;

    bool m_have_address;
    bool m_first_connection_maker_call;
    uint32_t m_listeningPort;
    uint32_t m_external_port;
    uint32_t m_ip_address;
    bool m_allow_local_ip;
    bool m_hide_my_port;

    System::Dispatcher& m_dispatcher;
    System::Event m_stopEvent;
    System::Event m_shutdownCompleteEvent;
    System::Timer m_idleTimer;
    System::TcpListener m_listener;
    Logging::LoggerRef logger;
    size_t m_spawnCount;
    std::atomic<bool> m_stop;

    cryptonote_protocol_handler& m_payload_handler;
    peerlist_manager m_peerlist;

    // OnceInInterval m_peer_handshake_idle_maker_interval;
    OnceInInterval m_connections_maker_interval;
    OnceInInterval m_peerlist_store_interval;
    System::Timer m_timedSyncTimer;

    std::string m_bind_ip;
    std::string m_port;
#ifdef ALLOW_DEBUG_COMMANDS
    uint64_t m_last_stat_request_time;
#endif
    std::vector<net_address> m_priority_peers;
    std::vector<net_address> m_exclusive_peers;
    std::vector<net_address> m_seed_nodes;
    std::list<peerlist_entry> m_command_line_peers;
    uint64_t m_peer_livetime;
    boost::uuids::uuid m_network_id;
    std::string m_p2pStatTrustedPubKey;
  };
}
