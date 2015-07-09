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

#include "net_node.h"

#include <algorithm>
#include <fstream>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/version.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

#include <System/EventLock.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <System/Ipv4Resolver.h>
#include <System/LatchGuard.h>
#include <System/TcpListener.h>
#include <System/TcpConnector.h>
 
#include "version.h"
#include "Common/util.h"
#include "crypto/crypto.h"

#include "p2p_protocol_defs.h"
#include "net_peerlist_boost_serialization.h"
#include "connection_context.h"
#include "LevinProtocol.h"

using namespace Logging;
using namespace CryptoNote;

namespace {

size_t get_random_index_with_fixed_probability(size_t max_index) {
  //divide by zero workaround
  if (!max_index)
    return 0;
  size_t x = crypto::rand<size_t>() % (max_index + 1);
  return (x*x*x) / (max_index*max_index); //parabola \/
}


void addPortMapping(Logging::LoggerRef& logger, uint32_t port) {
  // Add UPnP port mapping
  logger(INFO) << "Attempting to add IGD port mapping.";
  int result;
  UPNPDev* deviceList = upnpDiscover(1000, NULL, NULL, 0, 0, &result);
  UPNPUrls urls;
  IGDdatas igdData;
  char lanAddress[64];
  result = UPNP_GetValidIGD(deviceList, &urls, &igdData, lanAddress, sizeof lanAddress);
  freeUPNPDevlist(deviceList);
  if (result != 0) {
    if (result == 1) {
      std::ostringstream portString;
      portString << port;
      if (UPNP_AddPortMapping(urls.controlURL, igdData.first.servicetype, portString.str().c_str(),
        portString.str().c_str(), lanAddress, CryptoNote::CRYPTONOTE_NAME, "TCP", 0, "0") != 0) {
        logger(ERROR) << "UPNP_AddPortMapping failed.";
      } else {
        logger(INFO, BRIGHT_GREEN) << "Added IGD port mapping.";
      }
    } else if (result == 2) {
      logger(INFO) << "IGD was found but reported as not connected.";
    } else if (result == 3) {
      logger(INFO) << "UPnP device was found but not recoginzed as IGD.";
    } else {
      logger(ERROR) << "UPNP_GetValidIGD returned an unknown result code.";
    }

    FreeUPNPUrls(&urls);
  } else {
    logger(INFO) << "No IGD was found.";
  }
}

bool parse_peer_from_string(net_address& pe, const std::string& node_addr) {
  return Common::parseIpAddressAndPort(pe.ip, pe.port, node_addr);
}

}


namespace CryptoNote
{
  namespace
  {
    const command_line::arg_descriptor<std::string> arg_p2p_bind_ip        = {"p2p-bind-ip", "Interface for p2p network protocol", "0.0.0.0"};
    const command_line::arg_descriptor<std::string> arg_p2p_bind_port      = {"p2p-bind-port", "Port for p2p network protocol", boost::to_string(CryptoNote::P2P_DEFAULT_PORT)};
    const command_line::arg_descriptor<uint32_t>    arg_p2p_external_port  = {"p2p-external-port", "External port for p2p network protocol (if port forwarding used with NAT)", 0};
    const command_line::arg_descriptor<bool>        arg_p2p_allow_local_ip = {"allow-local-ip", "Allow local ip add to peer list, mostly in debug purposes"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_peer   = {"add-peer", "Manually add peer to local peerlist"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_priority_node   = {"add-priority-node", "Specify list of peers to connect to and attempt to keep the connection open"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_exclusive_node   = {"add-exclusive-node", "Specify list of peers to connect to only."
                                                                                                  " If this option is given the options add-priority-node and seed-node are ignored"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_seed_node   = {"seed-node", "Connect to a node to retrieve peer addresses, and disconnect"};
    const command_line::arg_descriptor<bool> arg_p2p_hide_my_port   =    {"hide-my-port", "Do not announce yourself as peerlist candidate", false, true};
  }

  std::string print_peerlist_to_string(const std::list<peerlist_entry>& pl) {
    time_t now_time = 0;
    time(&now_time);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(8) << std::hex << std::noshowbase;
    for (const auto& pe : pl) {
      ss << pe.id << "\t" << pe.adr << " \tlast_seen: " << Common::timeIntervalToString(now_time - pe.last_seen) << std::endl;
    }
    return ss.str();
  }


  template <typename Command, typename Handler>
  int invokeAdaptor(const std::string& reqBuf, std::string& resBuf, p2p_connection_context& ctx, Handler handler) {
    typedef typename Command::request Request;
    typedef typename Command::response Response;
    int command = Command::ID;

    Request req = boost::value_initialized<Request>();

    if (!LevinProtocol::decode(reqBuf, req)) {
      throw std::runtime_error("Failed to load_from_binary in command " + std::to_string(command));
    }

    Response res = boost::value_initialized<Response>();
    int ret = handler(command, req, res, ctx);
    resBuf = LevinProtocol::encode(res);
    return ret;
  }

  node_server::node_server(System::Dispatcher& dispatcher, CryptoNote::cryptonote_protocol_handler& payload_handler, Logging::ILogger& log) :
    m_dispatcher(dispatcher),
    m_payload_handler(payload_handler),
    m_allow_local_ip(false),
    m_hide_my_port(false),
    m_network_id(BYTECOIN_NETWORK),
    logger(log, "node_server"),
    m_stopEvent(m_dispatcher),
    m_shutdownCompleteEvent(m_dispatcher),
    m_idleTimer(m_dispatcher),
    m_timedSyncTimer(m_dispatcher),
    m_spawnCount(0),
    m_stop(false),
    // intervals
    // m_peer_handshake_idle_maker_interval(CryptoNote::P2P_DEFAULT_HANDSHAKE_INTERVAL),
    m_connections_maker_interval(1),
    m_peerlist_store_interval(60*30, false) {
  }


#define INVOKE_HANDLER(CMD, Handler) case CMD::ID: { ret = invokeAdaptor<CMD>(cmd.buf, out, ctx,  boost::bind(Handler, this, _1, _2, _3, _4)); break; }

  int node_server::handleCommand(const LevinProtocol::Command& cmd, std::string& out, p2p_connection_context& ctx, bool& handled) {
    int ret = 0;
    handled = true;

    if (cmd.isResponse && cmd.command == COMMAND_TIMED_SYNC::ID) {
      if (!handleTimedSyncResponse(cmd.buf, ctx)) {
        // invalid response, close connection
        ctx.m_state = cryptonote_connection_context::state_shutdown;
      }
      return 0;
    }

    switch (cmd.command) {
      INVOKE_HANDLER(COMMAND_HANDSHAKE, &node_server::handle_handshake)
      INVOKE_HANDLER(COMMAND_TIMED_SYNC, &node_server::handle_timed_sync)
      INVOKE_HANDLER(COMMAND_PING, &node_server::handle_ping)
#ifdef ALLOW_DEBUG_COMMANDS
      INVOKE_HANDLER(COMMAND_REQUEST_STAT_INFO, &node_server::handle_get_stat_info)
      INVOKE_HANDLER(COMMAND_REQUEST_NETWORK_STATE, &node_server::handle_get_network_state)
      INVOKE_HANDLER(COMMAND_REQUEST_PEER_ID, &node_server::handle_get_peer_id)
#endif
    default: {
        handled = false;
        ret = m_payload_handler.handleCommand(cmd.isNotify, cmd.command, cmd.buf, out, ctx, handled);
      }
    }

    return ret;
  }

#undef INVOKE_HANDLER

  //-----------------------------------------------------------------------------------
  
  void node_server::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_p2p_bind_ip);
    command_line::add_arg(desc, arg_p2p_bind_port);
    command_line::add_arg(desc, arg_p2p_external_port);
    command_line::add_arg(desc, arg_p2p_allow_local_ip);
    command_line::add_arg(desc, arg_p2p_add_peer);
    command_line::add_arg(desc, arg_p2p_add_priority_node);
    command_line::add_arg(desc, arg_p2p_add_exclusive_node);
    command_line::add_arg(desc, arg_p2p_seed_node);    
    command_line::add_arg(desc, arg_p2p_hide_my_port);
  }
  //-----------------------------------------------------------------------------------
  
  bool node_server::init_config() {
    try {
      std::string state_file_path = m_config_folder + "/" + CryptoNote::parameters::P2P_NET_DATA_FILENAME;
      std::ifstream p2p_data;
      p2p_data.open(state_file_path, std::ios_base::binary | std::ios_base::in);
      
      if (!p2p_data.fail()) {
        boost::archive::binary_iarchive a(p2p_data);
        a >> *this;
      } else {
        make_default_config();
      }

      //at this moment we have hardcoded config
      m_config.m_net_config.handshake_interval = CryptoNote::P2P_DEFAULT_HANDSHAKE_INTERVAL;
      m_config.m_net_config.connections_count = CryptoNote::P2P_DEFAULT_CONNECTIONS_COUNT;
      m_config.m_net_config.packet_max_size = CryptoNote::P2P_DEFAULT_PACKET_MAX_SIZE; //20 MB limit
      m_config.m_net_config.config_id = 0; // initial config
      m_config.m_net_config.connection_timeout = CryptoNote::P2P_DEFAULT_CONNECTION_TIMEOUT;
      m_config.m_net_config.ping_connection_timeout = CryptoNote::P2P_DEFAULT_PING_CONNECTION_TIMEOUT;
      m_config.m_net_config.send_peerlist_sz = CryptoNote::P2P_DEFAULT_PEERS_IN_HANDSHAKE;

      m_first_connection_maker_call = true;
    } catch (const std::exception& e) {
      logger(ERROR) << "init_config failed: " << e.what();
      return false;
    }
    return true;
  }

  //----------------------------------------------------------------------------------- 
  void node_server::for_each_connection(std::function<void(cryptonote_connection_context&, peerid_type)> f)
  {
    for (auto& ctx : m_connections) {
      f(ctx.second, ctx.second.peer_id);
    }
  }

  //----------------------------------------------------------------------------------- 
  void node_server::externalRelayNotifyToAll(int command, const std::string& data_buff) {
    m_dispatcher.remoteSpawn([this, command, data_buff] {
      relay_notify_to_all(command, data_buff, nullptr);
    });
  }

  //-----------------------------------------------------------------------------------
  bool node_server::make_default_config()
  {
    m_config.m_peer_id  = crypto::rand<uint64_t>();
    return true;
  }
  
  //-----------------------------------------------------------------------------------
  
  bool node_server::handle_command_line(const boost::program_options::variables_map& vm)
  {
    m_bind_ip = command_line::get_arg(vm, arg_p2p_bind_ip);
    m_port = command_line::get_arg(vm, arg_p2p_bind_port);
    m_external_port = command_line::get_arg(vm, arg_p2p_external_port);
    m_allow_local_ip = command_line::get_arg(vm, arg_p2p_allow_local_ip);

    if (command_line::has_arg(vm, arg_p2p_add_peer))
    {       
      std::vector<std::string> perrs = command_line::get_arg(vm, arg_p2p_add_peer);
      for(const std::string& pr_str: perrs)
      {
        peerlist_entry pe = AUTO_VAL_INIT(pe);
        pe.id = crypto::rand<uint64_t>();
        bool r = parse_peer_from_string(pe.adr, pr_str);
        if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to parse address from string: " << pr_str; return false; }
        m_command_line_peers.push_back(pe);
      }
    }

    if (command_line::has_arg(vm,arg_p2p_add_exclusive_node)) {
      if (!parse_peers_and_add_to_container(vm, arg_p2p_add_exclusive_node, m_exclusive_peers))
        return false;
    }
    if (command_line::has_arg(vm, arg_p2p_add_priority_node)) {
      if (!parse_peers_and_add_to_container(vm, arg_p2p_add_priority_node, m_priority_peers))
        return false;
    }
    if (command_line::has_arg(vm, arg_p2p_seed_node)) {
      if (!parse_peers_and_add_to_container(vm, arg_p2p_seed_node, m_seed_nodes))
        return false;
    }

    if (command_line::has_arg(vm, arg_p2p_hide_my_port)) {
      m_hide_my_port = true;
    }

    return true;
  }

  bool node_server::handleConfig(const NetNodeConfig& config) {
    m_bind_ip = config.bindIp;
    m_port = config.bindPort;
    m_external_port = config.externalPort;
    m_allow_local_ip = config.allowLocalIp;

    std::copy(config.peers.begin(), config.peers.end(), std::back_inserter(m_command_line_peers));
    std::copy(config.exclusiveNodes.begin(), config.exclusiveNodes.end(), std::back_inserter(m_exclusive_peers));
    std::copy(config.priorityNodes.begin(), config.priorityNodes.end(), std::back_inserter(m_priority_peers));
    std::copy(config.seedNodes.begin(), config.seedNodes.end(), std::back_inserter(m_seed_nodes));

    m_hide_my_port = config.hideMyPort;
    return true;
  }

  bool node_server::append_net_address(std::vector<net_address>& nodes, const std::string& addr) {
    size_t pos = addr.find_last_of(':');
    if (!(std::string::npos != pos && addr.length() - 1 != pos && 0 != pos)) {
      logger(ERROR, BRIGHT_RED) << "Failed to parse seed address from string: '" << addr << '\'';
      return false;
    }
    
    std::string host = addr.substr(0, pos);

    try {
      uint32_t port = Common::fromString<uint32_t>(addr.substr(pos + 1));

      System::Ipv4Resolver resolver(m_dispatcher);
      auto addr = resolver.resolve(host);
      nodes.push_back(net_address{hostToNetwork(addr.getValue()), port});

      logger(TRACE) << "Added seed node: " << nodes.back() << " (" << host << ")";

    } catch (const std::exception& e) {
      logger(ERROR, BRIGHT_YELLOW) << "Failed to resolve host name '" << host << "': " << e.what();
      return false;
    }

    return true;
  }


  //-----------------------------------------------------------------------------------
  
  bool node_server::init(const NetNodeConfig& config, bool testnet) {
    if (!testnet) {
      for (auto seed : CryptoNote::SEED_NODES) {
        append_net_address(m_seed_nodes, seed);
      }
    } else {
      m_network_id.data[0] += 1;
    }

    if (!handleConfig(config)) { 
      logger(ERROR, BRIGHT_RED) << "Failed to handle command line"; 
      return false; 
    }
    m_config_folder = config.configFolder;

    if (!init_config()) { 
      logger(ERROR, BRIGHT_RED) << "Failed to init config."; 
      return false; 
    }

    if (!m_peerlist.init(m_allow_local_ip)) { 
      logger(ERROR, BRIGHT_RED) << "Failed to init peerlist."; 
      return false; 
    }

    for(auto& p: m_command_line_peers)
      m_peerlist.append_with_peer_white(p);
    
    //only in case if we really sure that we have external visible ip
    m_have_address = true;
    m_ip_address = 0;
    m_last_stat_request_time = 0;

    //configure self
    // m_net_server.get_config_object().m_pcommands_handler = this;
    // m_net_server.get_config_object().m_invoke_timeout = CryptoNote::P2P_DEFAULT_INVOKE_TIMEOUT;

    //try to bind
    logger(INFO) << "Binding on " << m_bind_ip << ":" << m_port;
    m_listeningPort = Common::fromString<uint16_t>(m_port);

    m_listener = System::TcpListener(m_dispatcher, System::Ipv4Address(m_bind_ip), static_cast<uint16_t>(m_listeningPort));

    logger(INFO, BRIGHT_GREEN) << "Net service binded on " << m_bind_ip << ":" << m_listeningPort;

    if(m_external_port)
      logger(INFO) << "External port defined as " << m_external_port;

    addPortMapping(logger, m_listeningPort);

    return true;
  }
  //-----------------------------------------------------------------------------------
  
  CryptoNote::cryptonote_protocol_handler& node_server::get_payload_object()
  {
    return m_payload_handler;
  }
  //-----------------------------------------------------------------------------------
  
  bool node_server::run() {
    logger(INFO) << "Starting node_server";

    ++m_spawnCount;
    m_dispatcher.spawn(std::bind(&node_server::acceptLoop, this));

    ++m_spawnCount;
    m_dispatcher.spawn(std::bind(&node_server::onIdle, this));

    ++m_spawnCount;
    m_dispatcher.spawn(std::bind(&node_server::timedSyncLoop, this));

    m_stopEvent.wait();

    logger(INFO) << "Stopping node_server...";

    m_listener.stop();
    m_idleTimer.stop();
    m_timedSyncTimer.stop();

    logger(INFO) << "Stopping " << m_connections.size() + m_raw_connections.size() << " connections";

    for (auto& conn : m_raw_connections) {
      conn.second.connection.stop();
    }

    for (auto& conn : m_connections) {
      conn.second.connection.stop();
    }

    m_shutdownCompleteEvent.wait();

    logger(INFO) << "net_service loop stopped";
    return true;
  }

  //-----------------------------------------------------------------------------------
  
  uint64_t node_server::get_connections_count() {
    return m_connections.size();
  }
  //-----------------------------------------------------------------------------------
  
  bool node_server::deinit()  {
    return store_config();
  }

  //-----------------------------------------------------------------------------------
  
  bool node_server::store_config()
  {
    try {
      if (!tools::create_directories_if_necessary(m_config_folder)) {
        logger(INFO) << "Failed to create data directory: " << m_config_folder;
        return false;
      }

      std::string state_file_path = m_config_folder + "/" + CryptoNote::parameters::P2P_NET_DATA_FILENAME;
      std::ofstream p2p_data;
      p2p_data.open(state_file_path, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
      if (p2p_data.fail())  {
        logger(INFO) << "Failed to save config to file " << state_file_path;
        return false;
      };

      boost::archive::binary_oarchive a(p2p_data);
      a << *this;
      return true;
    } catch (const std::exception& e) {
      logger(WARNING) << "store_config failed: " << e.what();
    }

    return false;
  }
  //-----------------------------------------------------------------------------------
  
  bool node_server::send_stop_signal()  {
    m_stop = true;

    m_dispatcher.remoteSpawn([this] {
      m_stopEvent.set();
      m_payload_handler.stop();
    });

    logger(INFO, BRIGHT_YELLOW) << "Stop signal sent";
    return true;
  }

  //----------------------------------------------------------------------------------- 
  bool node_server::handshake(CryptoNote::LevinProtocol& proto, p2p_connection_context& context, bool just_take_peerlist) {
    COMMAND_HANDSHAKE::request arg;
    COMMAND_HANDSHAKE::response rsp;
    get_local_node_data(arg.node_data);
    m_payload_handler.get_payload_sync_data(arg.payload_data);

    proto.invoke(COMMAND_HANDSHAKE::ID, arg, rsp);

    if (rsp.node_data.network_id != m_network_id) {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE Failed, wrong network!  (" << rsp.node_data.network_id << "), closing connection.";
      return false;
    }

    if (!handle_remote_peerlist(rsp.local_peerlist, rsp.node_data.local_time, context)) {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE: failed to handle_remote_peerlist(...), closing connection.";
      return false;
    }

    if (just_take_peerlist) {
      return true;
    }

    if (!m_payload_handler.process_payload_sync_data(rsp.payload_data, context, true)) {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE invoked, but process_payload_sync_data returned false, dropping connection.";
      return false;
    }

    context.peer_id = rsp.node_data.peer_id;
    m_peerlist.set_peer_just_seen(rsp.node_data.peer_id, context.m_remote_ip, context.m_remote_port);

    if (rsp.node_data.peer_id == m_config.m_peer_id)  {
      logger(Logging::TRACE) << context << "Connection to self detected, dropping connection";
      return false;
    }

    logger(Logging::DEBUGGING) << context << "COMMAND_HANDSHAKE INVOKED OK";
    return true;
  }

  
  bool node_server::timedSync() {   
    COMMAND_TIMED_SYNC::request arg = AUTO_VAL_INIT(arg);
    m_payload_handler.get_payload_sync_data(arg.payload_data);
    auto cmdBuf = LevinProtocol::encode<COMMAND_TIMED_SYNC::request>(arg);

    forEachConnection([&](p2p_connection_context& conn) {
      if (conn.peer_id && 
          (conn.m_state == cryptonote_connection_context::state_normal || 
           conn.m_state == cryptonote_connection_context::state_idle)) {
        try {
          System::LatchGuard latch(conn.writeLatch);
          System::EventLock lock(conn.connectionEvent);
          LevinProtocol(conn.connection).sendBuf(COMMAND_TIMED_SYNC::ID, cmdBuf, true, false);
        } catch (std::exception&) {
          logger(DEBUGGING) << conn << "Failed to send COMMAND_TIMED_SYNC";
        }
      }
    });

    return true;
  }

  bool node_server::handleTimedSyncResponse(const std::string& in, p2p_connection_context& context) {
    COMMAND_TIMED_SYNC::response rsp;
    if (!LevinProtocol::decode<COMMAND_TIMED_SYNC::response>(in, rsp)) {
      return false;
    }

    if (!handle_remote_peerlist(rsp.local_peerlist, rsp.local_time, context)) {
      logger(Logging::ERROR) << context << "COMMAND_TIMED_SYNC: failed to handle_remote_peerlist(...), closing connection.";
      return false;
    }

    if (!context.m_is_income) {
      m_peerlist.set_peer_just_seen(context.peer_id, context.m_remote_ip, context.m_remote_port);
    }

    if (!m_payload_handler.process_payload_sync_data(rsp.payload_data, context, false)) {
      return false;
    }

    return true;
  }

  void node_server::forEachConnection(std::function<void(p2p_connection_context&)> action) {

    // create copy of connection ids because the list can be changed during action
    std::vector<boost::uuids::uuid> connectionIds;
    connectionIds.reserve(m_connections.size());
    for (const auto& c : m_connections) {
      connectionIds.push_back(c.first);
    }

    for (const auto& connId : connectionIds) {
      auto it = m_connections.find(connId);
      if (it != m_connections.end()) {
        action(it->second);
      }
    }
  }

  //----------------------------------------------------------------------------------- 
  bool node_server::is_peer_used(const peerlist_entry& peer) {
    if(m_config.m_peer_id == peer.id)
      return true; //dont make connections to ourself

    for (const auto& kv : m_connections) {
      const auto& cntxt = kv.second;
      if(cntxt.peer_id == peer.id || (!cntxt.m_is_income && peer.adr.ip == cntxt.m_remote_ip && peer.adr.port == cntxt.m_remote_port)) {
        return true;
      }
    }
    return false;
  }
  //-----------------------------------------------------------------------------------
  
  bool node_server::is_addr_connected(const net_address& peer) {
    for (const auto& conn : m_connections) {
      if (!conn.second.m_is_income && peer.ip == conn.second.m_remote_ip && peer.port == conn.second.m_remote_port) {
        return true;
      }
    }
    return false;
  }


  bool node_server::try_to_connect_and_handshake_with_new_peer(const net_address& na, bool just_take_peerlist, uint64_t last_seen_stamp, bool white)  {

    logger(DEBUGGING) << "Connecting to " << na << " (white=" << white << ", last_seen: "
        << (last_seen_stamp ? Common::timeIntervalToString(time(NULL) - last_seen_stamp) : "never") << ")...";

    try {
      System::TcpConnector connector(m_dispatcher);
      
      System::Event timeoutEvent(m_dispatcher);
      System::Timer timeoutTimer(m_dispatcher);

      m_dispatcher.spawn([&](){
        try {
          timeoutTimer.sleep(std::chrono::milliseconds(m_config.m_net_config.connection_timeout));
          connector.stop();
        } catch (std::exception&) {}
        timeoutEvent.set();
      });

      System::TcpConnection connection;

      try {
        connection = connector.connect(System::Ipv4Address(Common::ipAddressToString(na.ip)), static_cast<uint16_t>(na.port));
      } catch (System::InterruptedException&) {
        timeoutEvent.wait();
        return false;
      } catch (std::exception&) {
        timeoutTimer.stop();
        timeoutEvent.wait();
        throw;
      }

      p2p_connection_context ctx(m_dispatcher, std::move(connection));

      timeoutTimer.stop();
      timeoutEvent.wait();

      // p2p_connection_context ctx(m_dispatcher, std::move(connector.connect()));

      ctx.m_connection_id = boost::uuids::random_generator()();
      ctx.m_remote_ip = na.ip;
      ctx.m_remote_port = na.port;
      ctx.m_is_income = false;
      ctx.m_started = time(nullptr);

      auto raw = m_raw_connections.emplace(ctx.m_connection_id, std::move(ctx)).first;
      try {
        CryptoNote::LevinProtocol proto(raw->second.connection);

        if (!handshake(proto, raw->second, just_take_peerlist)) {
          logger(WARNING) << "Failed to HANDSHAKE with peer " << na;
          m_raw_connections.erase(raw);
          return false;
        }
      } catch (...) {
        m_raw_connections.erase(raw);
        throw;
      }

      if (just_take_peerlist) {
        logger(Logging::DEBUGGING, Logging::BRIGHT_GREEN) << raw->second << "CONNECTION HANDSHAKED OK AND CLOSED.";
        m_raw_connections.erase(raw);
        return true;
      }

      peerlist_entry pe_local = AUTO_VAL_INIT(pe_local);
      pe_local.adr = na;
      pe_local.id = raw->second.peer_id;
      time(&pe_local.last_seen);
      m_peerlist.append_with_peer_white(pe_local);

      if (m_stop) {
        m_raw_connections.erase(raw);
        throw System::InterruptedException();
      }

      auto iter = m_connections.emplace(raw->first, std::move(raw->second)).first;
      m_raw_connections.erase(raw);
      const boost::uuids::uuid& connectionId = iter->first;
      p2p_connection_context& connectionContext = iter->second;

      ++m_spawnCount;
      m_dispatcher.spawn(std::bind(&node_server::connectionHandler, this, std::cref(connectionId), std::ref(connectionContext)));

      return true;
    } catch (System::InterruptedException&) {
      logger(DEBUGGING) << "Connection process interrupted";
      throw;
    } catch (const std::exception& e) {
      logger(DEBUGGING) << "Connection to " << na << " failed: " << e.what();
    }

    return false;
  }

  //-----------------------------------------------------------------------------------
  bool node_server::make_new_connection_from_peerlist(bool use_white_list)
  {
    size_t local_peers_count = use_white_list ? m_peerlist.get_white_peers_count():m_peerlist.get_gray_peers_count();
    if(!local_peers_count)
      return false;//no peers

    size_t max_random_index = std::min<uint64_t>(local_peers_count -1, 20);

    std::set<size_t> tried_peers;

    size_t try_count = 0;
    size_t rand_count = 0;
    while(rand_count < (max_random_index+1)*3 &&  try_count < 10 && !m_stop) {
      ++rand_count;
      size_t random_index = get_random_index_with_fixed_probability(max_random_index);
      if (!(random_index < local_peers_count)) { logger(ERROR, BRIGHT_RED) << "random_starter_index < peers_local.size() failed!!"; return false; }

      if(tried_peers.count(random_index))
        continue;

      tried_peers.insert(random_index);
      peerlist_entry pe = AUTO_VAL_INIT(pe);
      bool r = use_white_list ? m_peerlist.get_white_peer_by_index(pe, random_index):m_peerlist.get_gray_peer_by_index(pe, random_index);
      if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to get random peer from peerlist(white:" << use_white_list << ")"; return false; }

      ++try_count;

      if(is_peer_used(pe))
        continue;

      logger(DEBUGGING) << "Selected peer: " << pe.id << " " << pe.adr << " [white=" << use_white_list
                    << "] last_seen: " << (pe.last_seen ? Common::timeIntervalToString(time(NULL) - pe.last_seen) : "never");
      
      if(!try_to_connect_and_handshake_with_new_peer(pe.adr, false, pe.last_seen, use_white_list))
        continue;

      return true;
    }
    return false;
  }
  //-----------------------------------------------------------------------------------
  
  bool node_server::connections_maker()
  {
    if (!connect_to_peerlist(m_exclusive_peers)) {
      return false;
    }

    if (!m_exclusive_peers.empty()) {
      return true;
    }

    if(!m_peerlist.get_white_peers_count() && m_seed_nodes.size()) {
      size_t try_count = 0;
      size_t current_index = crypto::rand<size_t>()%m_seed_nodes.size();
      
      while(true) {        
        if(try_to_connect_and_handshake_with_new_peer(m_seed_nodes[current_index], true))
          break;

        if(++try_count > m_seed_nodes.size()) {
          logger(ERROR) << "Failed to connect to any of seed peers, continuing without seeds";
          break;
        }
        if(++current_index >= m_seed_nodes.size())
          current_index = 0;
      }
    }

    if (!connect_to_peerlist(m_priority_peers)) return false;

    size_t expected_white_connections = (m_config.m_net_config.connections_count * CryptoNote::P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT) / 100;

    size_t conn_count = get_outgoing_connections_count();
    if(conn_count < m_config.m_net_config.connections_count)
    {
      if(conn_count < expected_white_connections)
      {
        //start from white list
        if(!make_expected_connections_count(true, expected_white_connections))
          return false;
        //and then do grey list
        if(!make_expected_connections_count(false, m_config.m_net_config.connections_count))
          return false;
      }else
      {
        //start from grey list
        if(!make_expected_connections_count(false, m_config.m_net_config.connections_count))
          return false;
        //and then do white list
        if(!make_expected_connections_count(true, m_config.m_net_config.connections_count))
          return false;
      }
    }

    return true;
  }
  //-----------------------------------------------------------------------------------
  
  bool node_server::make_expected_connections_count(bool white_list, size_t expected_connections)
  {
    size_t conn_count = get_outgoing_connections_count();
    //add new connections from white peers
    while(conn_count < expected_connections)
    {
      if(m_stopEvent.get())
        return false;

      if(!make_new_connection_from_peerlist(white_list))
        break;
      conn_count = get_outgoing_connections_count();
    }
    return true;
  }

  //-----------------------------------------------------------------------------------
  size_t node_server::get_outgoing_connections_count() {
    size_t count = 0;
    for (const auto& cntxt : m_connections) {
      if (!cntxt.second.m_is_income)
        ++count;
    }
    return count;
  }

  //-----------------------------------------------------------------------------------
  bool node_server::idle_worker() {
    m_connections_maker_interval.call(std::bind(&node_server::connections_maker, this));
    m_peerlist_store_interval.call(std::bind(&node_server::store_config, this));
    return true;
  }

  //-----------------------------------------------------------------------------------
  bool node_server::fix_time_delta(std::list<peerlist_entry>& local_peerlist, time_t local_time, int64_t& delta)
  {
    //fix time delta
    time_t now = 0;
    time(&now);
    delta = now - local_time;

    BOOST_FOREACH(peerlist_entry& be, local_peerlist)
    {
      if(be.last_seen > local_time)
      {
        logger(ERROR) << "FOUND FUTURE peerlist for entry " << be.adr << " last_seen: " << be.last_seen << ", local_time(on remote node):" << local_time;
        return false;
      }
      be.last_seen += delta;
    }
    return true;
  }

  //-----------------------------------------------------------------------------------
 
  bool node_server::handle_remote_peerlist(const std::list<peerlist_entry>& peerlist, time_t local_time, const cryptonote_connection_context& context)
  {
    int64_t delta = 0;
    std::list<peerlist_entry> peerlist_ = peerlist;
    if(!fix_time_delta(peerlist_, local_time, delta))
      return false;
    logger(Logging::TRACE) << context << "REMOTE PEERLIST: TIME_DELTA: " << delta << ", remote peerlist size=" << peerlist_.size();
    logger(Logging::TRACE) << context << "REMOTE PEERLIST: " <<  print_peerlist_to_string(peerlist_);
    return m_peerlist.merge_peerlist(peerlist_);
  }
  //-----------------------------------------------------------------------------------
  
  bool node_server::get_local_node_data(basic_node_data& node_data)
  {
    time_t local_time;
    time(&local_time);
    node_data.local_time = local_time;
    node_data.peer_id = m_config.m_peer_id;
    if(!m_hide_my_port)
      node_data.my_port = m_external_port ? m_external_port : m_listeningPort;
    else 
      node_data.my_port = 0;
    node_data.network_id = m_network_id;
    return true;
  }
  //-----------------------------------------------------------------------------------
#ifdef ALLOW_DEBUG_COMMANDS
  
  bool node_server::check_trust(const proof_of_trust& tr)
  {
    uint64_t local_time = time(NULL);
    uint64_t time_delata = local_time > tr.time ? local_time - tr.time: tr.time - local_time;
    if(time_delata > 24*60*60 )
    {
      logger(ERROR) << "check_trust failed to check time conditions, local_time=" <<  local_time << ", proof_time=" << tr.time;
      return false;
    }
    if(m_last_stat_request_time >= tr.time )
    {
      logger(ERROR) << "check_trust failed to check time conditions, last_stat_request_time=" <<  m_last_stat_request_time << ", proof_time=" << tr.time;
      return false;
    }
    if(m_config.m_peer_id != tr.peer_id)
    {
      logger(ERROR) << "check_trust failed: peer_id mismatch (passed " << tr.peer_id << ", expected " << m_config.m_peer_id<< ")";
      return false;
    }
    crypto::public_key pk = AUTO_VAL_INIT(pk);
    Common::podFromHex(CryptoNote::P2P_STAT_TRUSTED_PUB_KEY, pk);
    crypto::hash h = get_proof_of_trust_hash(tr);
    if(!crypto::check_signature(h, pk, tr.sign))
    {
      logger(ERROR) << "check_trust failed: sign check failed";
      return false;
    }
    //update last request time
    m_last_stat_request_time = tr.time;
    return true;
  }
  //-----------------------------------------------------------------------------------
  
  int node_server::handle_get_stat_info(int command, COMMAND_REQUEST_STAT_INFO::request& arg, COMMAND_REQUEST_STAT_INFO::response& rsp, p2p_connection_context& context)
  {
    if(!check_trust(arg.tr)) {
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }
    rsp.connections_count = get_connections_count();
    rsp.incoming_connections_count = rsp.connections_count - get_outgoing_connections_count();
    rsp.version = PROJECT_VERSION_LONG;
    rsp.os_version = tools::get_os_version_string();
    m_payload_handler.get_stat_info(rsp.payload_info);
    return 1;
  }
  //-----------------------------------------------------------------------------------
  
  int node_server::handle_get_network_state(int command, COMMAND_REQUEST_NETWORK_STATE::request& arg, COMMAND_REQUEST_NETWORK_STATE::response& rsp, p2p_connection_context& context)
  {
    if(!check_trust(arg.tr)) {
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }

    for (const auto& cntxt : m_connections) {
      connection_entry ce;
      ce.adr.ip = cntxt.second.m_remote_ip;
      ce.adr.port = cntxt.second.m_remote_port;
      ce.id = cntxt.second.peer_id;
      ce.is_income = cntxt.second.m_is_income;
      rsp.connections_list.push_back(ce);
    }

    m_peerlist.get_peerlist_full(rsp.local_peerlist_gray, rsp.local_peerlist_white);
    rsp.my_id = m_config.m_peer_id;
    rsp.local_time = time(NULL);
    return 1;
  }
  //-----------------------------------------------------------------------------------
  
  int node_server::handle_get_peer_id(int command, COMMAND_REQUEST_PEER_ID::request& arg, COMMAND_REQUEST_PEER_ID::response& rsp, p2p_connection_context& context)
  {
    rsp.my_id = m_config.m_peer_id;
    return 1;
  }
#endif
  
  //-----------------------------------------------------------------------------------
  
  void node_server::relay_notify_to_all(int command, const std::string& data_buff, const net_connection_id* excludeConnection) {
    net_connection_id excludeId = excludeConnection ? *excludeConnection : boost::value_initialized<net_connection_id>();

    forEachConnection([&](p2p_connection_context& conn) {
      if (conn.peer_id && conn.m_connection_id != excludeId) {
        try {
          logger(TRACE) << conn << "Relay command " << command;
          System::LatchGuard latch(conn.writeLatch);
          System::EventLock lock(conn.connectionEvent);
          LevinProtocol(conn.connection).sendBuf(command, data_buff, false);
        } catch (const std::exception& e) {
          logger(DEBUGGING) << conn << "Failed to relay notification id=" << command << ": " << e.what();
        }
      }
    });
  }
 
  //-----------------------------------------------------------------------------------
  bool node_server::invoke_notify_to_peer(int command, const std::string& req_buff, const cryptonote_connection_context& context) {
    auto it = m_connections.find(context.m_connection_id);
    if (it == m_connections.end()) {
      return false;
    }

    try {
      System::LatchGuard latch(it->second.writeLatch);
      System::EventLock lock(it->second.connectionEvent);
      LevinProtocol(it->second.connection).sendBuf(command, req_buff, false);
    } catch (const std::exception& e) {
      logger(DEBUGGING) << it->second << "Failed to invoke notification id=" << command << ": " << e.what();
    }

    return true;
  }

  //-----------------------------------------------------------------------------------
  bool node_server::try_ping(basic_node_data& node_data, p2p_connection_context& context)
  {
    if(!node_data.my_port)
      return false;

    uint32_t actual_ip =  context.m_remote_ip;
    if(!m_peerlist.is_ip_allowed(actual_ip))
      return false;

    std::string ip = Common::ipAddressToString(actual_ip);
    auto port = node_data.my_port;
    peerid_type pr = node_data.peer_id;

    try {
      System::TcpConnector connector(m_dispatcher);
      System::TcpConnection conn = connector.connect(System::Ipv4Address(ip), static_cast<uint16_t>(port));

      LevinProtocol proto(conn);

      COMMAND_PING::request req;
      COMMAND_PING::response rsp;
      proto.invoke(COMMAND_PING::ID, req, rsp);

      if (rsp.status != PING_OK_RESPONSE_STATUS_TEXT || pr != rsp.peer_id) {
        logger(Logging::DEBUGGING) << context << "back ping invoke wrong response \"" << rsp.status << "\" from" << ip << ":" << port << ", hsh_peer_id=" << pr << ", rsp.peer_id=" << rsp.peer_id;
        return false;
      }

      return true;

    } catch (std::exception& e) {
      logger(Logging::DEBUGGING) << context << "back ping to " << ip << ":" << port << " failed: " << e.what();
    }

    return false;
  }

  //----------------------------------------------------------------------------------- 
  int node_server::handle_timed_sync(int command, COMMAND_TIMED_SYNC::request& arg, COMMAND_TIMED_SYNC::response& rsp, p2p_connection_context& context)
  {
    if(!m_payload_handler.process_payload_sync_data(arg.payload_data, context, false)) {
      logger(Logging::ERROR) << context << "Failed to process_payload_sync_data(), dropping connection";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }

    //fill response
    rsp.local_time = time(NULL);
    m_peerlist.get_peerlist_head(rsp.local_peerlist);
    m_payload_handler.get_payload_sync_data(rsp.payload_data);
    logger(Logging::TRACE) << context << "COMMAND_TIMED_SYNC";
    return 1;
  }
  //-----------------------------------------------------------------------------------
  
  int node_server::handle_handshake(int command, COMMAND_HANDSHAKE::request& arg, COMMAND_HANDSHAKE::response& rsp, p2p_connection_context& context)
  {
    if(arg.node_data.network_id != m_network_id) {
      logger(Logging::INFO) << context << "WRONG NETWORK AGENT CONNECTED! id=" << arg.node_data.network_id;
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }

    if(!context.m_is_income) {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE came not from incoming connection";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }

    if(context.peer_id) {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE came, but seems that connection already have associated peer_id (double COMMAND_HANDSHAKE?)";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }

    if(!m_payload_handler.process_payload_sync_data(arg.payload_data, context, true))  {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE came, but process_payload_sync_data returned false, dropping connection.";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }
    //associate peer_id with this connection
    context.peer_id = arg.node_data.peer_id;

    if(arg.node_data.peer_id != m_config.m_peer_id && arg.node_data.my_port) {
      peerid_type peer_id_l = arg.node_data.peer_id;
      uint32_t port_l = arg.node_data.my_port;

      if (try_ping(arg.node_data, context)) {
          //called only(!) if success pinged, update local peerlist
          peerlist_entry pe;
          pe.adr.ip = context.m_remote_ip;
          pe.adr.port = port_l;
          time(&pe.last_seen);
          pe.id = peer_id_l;
          m_peerlist.append_with_peer_white(pe);

          logger(Logging::TRACE) << context << "PING SUCCESS " << Common::ipAddressToString(context.m_remote_ip) << ":" << port_l;
      }
    }

    //fill response
    m_peerlist.get_peerlist_head(rsp.local_peerlist);
    get_local_node_data(rsp.node_data);
    m_payload_handler.get_payload_sync_data(rsp.payload_data);

    logger(Logging::DEBUGGING, Logging::BRIGHT_GREEN) << "COMMAND_HANDSHAKE";
    return 1;
  }
  //-----------------------------------------------------------------------------------
  
  int node_server::handle_ping(int command, COMMAND_PING::request& arg, COMMAND_PING::response& rsp, p2p_connection_context& context)
  {
    logger(Logging::TRACE) << context << "COMMAND_PING";
    rsp.status = PING_OK_RESPONSE_STATUS_TEXT;
    rsp.peer_id = m_config.m_peer_id;
    return 1;
  }
  //-----------------------------------------------------------------------------------
  
  bool node_server::log_peerlist()
  {
    std::list<peerlist_entry> pl_wite;
    std::list<peerlist_entry> pl_gray;
    m_peerlist.get_peerlist_full(pl_gray, pl_wite);
    logger(INFO) << ENDL << "Peerlist white:" << ENDL << print_peerlist_to_string(pl_wite) << ENDL << "Peerlist gray:" << ENDL << print_peerlist_to_string(pl_gray) ;
    return true;
  }
  //-----------------------------------------------------------------------------------
  
  bool node_server::log_connections() {
    logger(INFO) << "Connections: \r\n" << print_connections_container() ;
    return true;
  }
  //-----------------------------------------------------------------------------------
  
  std::string node_server::print_connections_container() {

    std::stringstream ss;

    for (const auto& cntxt : m_connections) {
      ss << Common::ipAddressToString(cntxt.second.m_remote_ip) << ":" << cntxt.second.m_remote_port
        << " \t\tpeer_id " << cntxt.second.peer_id
        << " \t\tconn_id " << cntxt.second.m_connection_id << (cntxt.second.m_is_income ? " INC" : " OUT")
        << std::endl;
    }

    return ss.str();
  }
  //-----------------------------------------------------------------------------------
  
  void node_server::on_connection_new(p2p_connection_context& context)
  {
    logger(TRACE) << context << "NEW CONNECTION";
    m_payload_handler.onConnectionOpened(context);
  }
  //-----------------------------------------------------------------------------------
  
  void node_server::on_connection_close(p2p_connection_context& context)
  {
    logger(TRACE) << context << "CLOSE CONNECTION";
    m_payload_handler.onConnectionClosed(context);
  }
  
  bool node_server::is_priority_node(const net_address& na)
  {
    return 
      (std::find(m_priority_peers.begin(), m_priority_peers.end(), na) != m_priority_peers.end()) || 
      (std::find(m_exclusive_peers.begin(), m_exclusive_peers.end(), na) != m_exclusive_peers.end());
  }

  bool node_server::connect_to_peerlist(const std::vector<net_address>& peers)
  {
    for(const auto& na: peers) {
      if (!is_addr_connected(na)) {
        try_to_connect_and_handshake_with_new_peer(na);
      }
    }

    return true;
  }

  bool node_server::parse_peers_and_add_to_container(const boost::program_options::variables_map& vm, 
    const command_line::arg_descriptor<std::vector<std::string> > & arg, std::vector<net_address>& container)
  {
    std::vector<std::string> perrs = command_line::get_arg(vm, arg);

    for(const std::string& pr_str: perrs) {
      net_address na = AUTO_VAL_INIT(na);
      if (!parse_peer_from_string(na, pr_str)) { 
        logger(ERROR, BRIGHT_RED) << "Failed to parse address from string: " << pr_str; 
        return false; 
      }
      container.push_back(na);
    }

    return true;
  }

  void node_server::acceptLoop() {
    try {
      for (;;) {
        p2p_connection_context ctx(m_dispatcher, m_listener.accept());
        ctx.m_connection_id = boost::uuids::random_generator()();
        ctx.m_is_income = true;
        ctx.m_started = time(nullptr);

        auto addressAndPort = ctx.connection.getPeerAddressAndPort();
        ctx.m_remote_ip = hostToNetwork(addressAndPort.first.getValue());
        ctx.m_remote_port = addressAndPort.second;

        auto iter = m_connections.emplace(ctx.m_connection_id, std::move(ctx)).first;
        const boost::uuids::uuid& connectionId = iter->first;
        p2p_connection_context& connection = iter->second;

        ++m_spawnCount;
        m_dispatcher.spawn(std::bind(&node_server::connectionHandler, this, std::cref(connectionId), std::ref(connection)));
      }
    } catch (System::InterruptedException&) {
    } catch (const std::exception& e) {
      logger(WARNING) << "Exception in acceptLoop: " << e.what();
    }

    logger(DEBUGGING) << "acceptLoop finished";

    if (--m_spawnCount == 0) {
      m_shutdownCompleteEvent.set();
    }
  }

  void node_server::onIdle() {
    logger(DEBUGGING) << "onIdle started";

    try {
      while (!m_stop) {
        idle_worker();
        m_payload_handler.on_idle();
        m_idleTimer.sleep(std::chrono::seconds(1));
      }
    } catch (System::InterruptedException&) {
    } catch (std::exception& e) {
      logger(WARNING) << "Exception in onIdle: " << e.what();
    }

    logger(DEBUGGING) << "onIdle finished";

    if (--m_spawnCount == 0) {
      m_shutdownCompleteEvent.set();
    }
  }

  void node_server::timedSyncLoop() {
    try {
      for (;;) {
        m_timedSyncTimer.sleep(std::chrono::seconds(P2P_DEFAULT_HANDSHAKE_INTERVAL));
        timedSync();
      }
    } catch (System::InterruptedException&) {
    } catch (std::exception& e) {
      logger(WARNING) << "Exception in timedSyncLoop: " << e.what();
    }

    logger(DEBUGGING) << "timedSyncLoop finished";

    if (--m_spawnCount == 0) {
      m_shutdownCompleteEvent.set();
    }
  }

  void node_server::connectionHandler(const boost::uuids::uuid& connectionId, p2p_connection_context& ctx) {

    try {
      on_connection_new(ctx);

      LevinProtocol proto(ctx.connection);
      LevinProtocol::Command cmd;

      for (;;) {

        if (ctx.m_state == cryptonote_connection_context::state_sync_required) {
          ctx.m_state = cryptonote_connection_context::state_synchronizing;
          m_payload_handler.start_sync(ctx);
        }

        if (!proto.readCommand(cmd)) {
          break;
        }

        std::string response;
        bool handled = false;
        auto retcode = handleCommand(cmd, response, ctx, handled);

        // send response
        if (cmd.needReply()) {
          System::LatchGuard latch(ctx.writeLatch);
          System::EventLock lock(ctx.connectionEvent);
          if (handled) {
            proto.sendReply(cmd.command, response, retcode);
          } else {
            proto.sendReply(cmd.command, std::string(), static_cast<int32_t>(LevinError::ERROR_CONNECTION_HANDLER_NOT_DEFINED));
          }
        }

        if (ctx.m_state == cryptonote_connection_context::state_shutdown) {
          break;
        }
      }
      
    } catch (System::InterruptedException&) {
      logger(TRACE) << "Closing connection...";
    } catch (std::exception& e) {
      logger(WARNING) << "Exception in connectionHandler: " << e.what();
    }

    ctx.writeLatch.wait();

    on_connection_close(ctx);
    m_connections.erase(connectionId);
 
    if (--m_spawnCount == 0) {
      m_shutdownCompleteEvent.set();
    }

  }
   

}
