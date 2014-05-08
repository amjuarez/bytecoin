// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "version.h"
#include "string_tools.h"
#include "common/command_line.h"
#include "common/util.h"
#include "net/net_helper.h"
#include "math_helper.h"
#include "p2p_protocol_defs.h"
#include "net_peerlist_boost_serialization.h"
#include "net/local_ip.h"
#include "crypto/crypto.h"
#include "storages/levin_abstract_invoke2.h"
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

#define NET_MAKE_IP(b1,b2,b3,b4)  ((LPARAM)(((DWORD)(b1)<<24)+((DWORD)(b2)<<16)+((DWORD)(b3)<<8)+((DWORD)(b4))))


namespace nodetool
{
  namespace
  {
    const command_line::arg_descriptor<std::string> arg_p2p_bind_ip        = {"p2p-bind-ip", "Interface for p2p network protocol", "0.0.0.0"};
    const command_line::arg_descriptor<std::string> arg_p2p_bind_port      = {"p2p-bind-port", "Port for p2p network protocol", boost::to_string(P2P_DEFAULT_PORT)};
    const command_line::arg_descriptor<uint32_t>    arg_p2p_external_port  = {"p2p-external-port", "External port for p2p network protocol (if port forwarding used with NAT)", 0};
    const command_line::arg_descriptor<bool>        arg_p2p_allow_local_ip = {"allow-local-ip", "Allow local ip add to peer list, mostly in debug purposes"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_peer   = {"add-peer", "Manually add peer to local peerlist"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_priority_node   = {"add-priority-node", "Specify list of peers to connect to and attempt to keep the connection open"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_seed_node   = {"seed-node", "Connect to a node to retrieve peer addresses, and disconnect"};
    const command_line::arg_descriptor<bool> arg_p2p_hide_my_port   =    {"hide-my-port", "Do not announce yourself as peerlist candidate", false, true};  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_p2p_bind_ip);
    command_line::add_arg(desc, arg_p2p_bind_port);
    command_line::add_arg(desc, arg_p2p_external_port);
    command_line::add_arg(desc, arg_p2p_allow_local_ip);
    command_line::add_arg(desc, arg_p2p_add_peer);
    command_line::add_arg(desc, arg_p2p_add_priority_node);
    command_line::add_arg(desc, arg_p2p_seed_node);    
    command_line::add_arg(desc, arg_p2p_hide_my_port);   }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::init_config()
  {
    //
    TRY_ENTRY();
    std::string state_file_path = m_config_folder + "/" + P2P_NET_DATA_FILENAME;
    std::ifstream p2p_data;
    p2p_data.open( state_file_path , std::ios_base::binary | std::ios_base::in);
    if(!p2p_data.fail())
    {
      boost::archive::binary_iarchive a(p2p_data);
      a >> *this;
    }else
    {
      make_default_config();
    }

    //at this moment we have hardcoded config
    m_config.m_net_config.handshake_interval = P2P_DEFAULT_HANDSHAKE_INTERVAL;
    m_config.m_net_config.connections_count = P2P_DEFAULT_CONNECTIONS_COUNT;
    m_config.m_net_config.packet_max_size = P2P_DEFAULT_PACKET_MAX_SIZE; //20 MB limit
    m_config.m_net_config.config_id = 0; // initial config
    m_config.m_net_config.connection_timeout = P2P_DEFAULT_CONNECTION_TIMEOUT;
    m_config.m_net_config.ping_connection_timeout = P2P_DEFAULT_PING_CONNECTION_TIMEOUT;
    m_config.m_net_config.send_peerlist_sz = P2P_DEFAULT_PEERS_IN_HANDSHAKE;

    m_first_connection_maker_call = true;
    CATCH_ENTRY_L0("node_server::init_config", false);
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::for_each_connection(std::function<bool(typename t_payload_net_handler::connection_context&, peerid_type)> f)
  {
    m_net_server.get_config_object().foreach_connection([&](p2p_connection_context& cntx){
      return f(cntx, cntx.peer_id);
    });
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::make_default_config()
  {
    m_config.m_peer_id  = crypto::rand<uint64_t>();
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::parse_peer_from_string(nodetool::net_address& pe, const std::string& node_addr)
  {
    return string_tools::parse_peer_from_string(pe.ip, pe.port, node_addr);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::handle_command_line(const boost::program_options::variables_map& vm)
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
        nodetool::peerlist_entry pe = AUTO_VAL_INIT(pe);
        pe.id = crypto::rand<uint64_t>();
        bool r = parse_peer_from_string(pe.adr, pr_str);
        CHECK_AND_ASSERT_MES(r, false, "Failed to parse address from string: " << pr_str);
        m_command_line_peers.push_back(pe);
      }
    }

    if (command_line::has_arg(vm, arg_p2p_add_priority_node))
    {       
      std::vector<std::string> perrs = command_line::get_arg(vm, arg_p2p_add_priority_node);
      for(const std::string& pr_str: perrs)
      {
        nodetool::net_address na = AUTO_VAL_INIT(na);
        bool r = parse_peer_from_string(na, pr_str);
        CHECK_AND_ASSERT_MES(r, false, "Failed to parse address from string: " << pr_str);
        m_priority_peers.push_back(na);
      }
    }
    if (command_line::has_arg(vm, arg_p2p_seed_node))
    {
      std::vector<std::string> seed_perrs = command_line::get_arg(vm, arg_p2p_seed_node);
      for(const std::string& pr_str: seed_perrs)
      {
        nodetool::net_address na = AUTO_VAL_INIT(na);
        bool r = parse_peer_from_string(na, pr_str);
        CHECK_AND_ASSERT_MES(r, false, "Failed to parse seed address from string: " << pr_str);
        m_seed_nodes.push_back(na);
      }
    }
    if(command_line::has_arg(vm, arg_p2p_hide_my_port))
      m_hide_my_port = true;    return true;
  }
  //-----------------------------------------------------------------------------------
  namespace
  {
    template<typename T>
    bool append_net_address(T& nodes, const std::string& addr)
    {
      using namespace boost::asio;

      size_t pos = addr.find_last_of(':');
      CHECK_AND_ASSERT_MES(std::string::npos != pos && addr.length() - 1 != pos && 0 != pos, false, "Failed to parse seed address from string: '" << addr << '\'');
      std::string host = addr.substr(0, pos);
      std::string port = addr.substr(pos + 1);

      io_service io_srv;
      ip::tcp::resolver resolver(io_srv);
      ip::tcp::resolver::query query(host, port);
      boost::system::error_code ec;
      ip::tcp::resolver::iterator i = resolver.resolve(query, ec);
      CHECK_AND_NO_ASSERT_MES(!ec, false, "Failed to resolve host name '" << host << "': " << ec.message() << ':' << ec.value());

      ip::tcp::resolver::iterator iend;
      for (; i != iend; ++i)
      {
        ip::tcp::endpoint endpoint = *i;
        if (endpoint.address().is_v4())
        {
          nodetool::net_address na;
          na.ip = boost::asio::detail::socket_ops::host_to_network_long(endpoint.address().to_v4().to_ulong());
          na.port = endpoint.port();
          nodes.push_back(na);
          LOG_PRINT_L4("Added seed node: " << endpoint.address().to_v4().to_string(ec) << ':' << na.port);
        }
        else
        {
          LOG_PRINT_L2("IPv6 doesn't supported, skip '" << host << "' -> " << endpoint.address().to_v6().to_string(ec));
        }
      }

      return true;
    }
  }

  #define ADD_HARDCODED_SEED_NODE(addr) append_net_address(m_seed_nodes, addr);
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::init(const boost::program_options::variables_map& vm)
  {
    ADD_HARDCODED_SEED_NODE("107.158.233.98:18080");
    ADD_HARDCODED_SEED_NODE("64.22.111.2:18080");

    bool res = handle_command_line(vm);
    CHECK_AND_ASSERT_MES(res, false, "Failed to handle command line");
    m_config_folder = command_line::get_arg(vm, command_line::arg_data_dir);

    res = init_config();
    CHECK_AND_ASSERT_MES(res, false, "Failed to init config.");

    res = m_peerlist.init(m_allow_local_ip);
    CHECK_AND_ASSERT_MES(res, false, "Failed to init peerlist.");


    for(auto& p: m_command_line_peers)
      m_peerlist.append_with_peer_white(p);
    
    //only in case if we really sure that we have external visible ip
    m_have_address = true;
    m_ip_address = 0;
    m_last_stat_request_time = 0;

    //configure self
    m_net_server.set_threads_prefix("P2P");
    m_net_server.get_config_object().m_pcommands_handler = this;
    m_net_server.get_config_object().m_invoke_timeout = P2P_DEFAULT_INVOKE_TIMEOUT;

    //try to bind
    LOG_PRINT_L0("Binding on " << m_bind_ip << ":" << m_port);
    res = m_net_server.init_server(m_port, m_bind_ip);
    CHECK_AND_ASSERT_MES(res, false, "Failed to bind server");

    m_listenning_port = m_net_server.get_binded_port();
    LOG_PRINT_GREEN("Net service binded on " << m_bind_ip << ":" << m_listenning_port, LOG_LEVEL_0);
    if(m_external_port)
      LOG_PRINT_L0("External port defined as " << m_external_port);

    // Add UPnP port mapping
    LOG_PRINT_L0("Attempting to add IGD port mapping.");
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
        portString << m_listenning_port;
        if (UPNP_AddPortMapping(urls.controlURL, igdData.first.servicetype, portString.str().c_str(), portString.str().c_str(), lanAddress, CRYPTONOTE_NAME, "TCP", 0, "0") != 0) {
          LOG_ERROR("UPNP_AddPortMapping failed.");
        } else {
          LOG_PRINT_GREEN("Added IGD port mapping.", LOG_LEVEL_0);
        }
      } else if (result == 2) {
        LOG_PRINT_L0("IGD was found but reported as not connected.");
      } else if (result == 3) {
        LOG_PRINT_L0("UPnP device was found but not recoginzed as IGD.");
      } else {
        LOG_ERROR("UPNP_GetValidIGD returned an unknown result code.");
      }

      FreeUPNPUrls(&urls);
    } else {
      LOG_PRINT_L0("No IGD was found.");
    }

    return res;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  typename node_server<t_payload_net_handler>::payload_net_handler& node_server<t_payload_net_handler>::get_payload_object()
  {
    return m_payload_handler;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::run()
  {
    //here you can set worker threads count
    int thrds_count = 10;

    m_net_server.add_idle_handler(boost::bind(&node_server<t_payload_net_handler>::idle_worker, this), 1000);
    m_net_server.add_idle_handler(boost::bind(&t_payload_net_handler::on_idle, &m_payload_handler), 1000);

    boost::thread::attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);

    //go to loop
    LOG_PRINT("Run net_service loop( " << thrds_count << " threads)...", LOG_LEVEL_0);
    if(!m_net_server.run_server(thrds_count, true, attrs))
    {
      LOG_ERROR("Failed to run net tcp server!");
    }

    LOG_PRINT("net_service loop stopped.", LOG_LEVEL_0);
    return true;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  uint64_t node_server<t_payload_net_handler>::get_connections_count()
  {
    return m_net_server.get_config_object().get_connections_count();
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::deinit()
  {
    m_peerlist.deinit();
    m_net_server.deinit_server();
    return store_config();
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::store_config()
  {

    TRY_ENTRY();
    if (!tools::create_directories_if_necessary(m_config_folder))
    {
      LOG_PRINT_L0("Failed to create data directory: " << m_config_folder);
      return false;
    }

    std::string state_file_path = m_config_folder + "/" + P2P_NET_DATA_FILENAME;
    std::ofstream p2p_data;
    p2p_data.open( state_file_path , std::ios_base::binary | std::ios_base::out| std::ios::trunc);
    if(p2p_data.fail())
    {
      LOG_PRINT_L0("Failed to save config to file " << state_file_path);
      return false;
    };

    boost::archive::binary_oarchive a(p2p_data);
    a << *this;
    return true;
    CATCH_ENTRY_L0("blockchain_storage::save", false);

    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::send_stop_signal()
  {
    m_net_server.send_stop_signal();
    LOG_PRINT_L0("[node] Stop signal sent");
    return true;
  }
  //-----------------------------------------------------------------------------------
 

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::do_handshake_with_peer(peerid_type& pi, p2p_connection_context& context_, bool just_take_peerlist)
  {
    typename COMMAND_HANDSHAKE::request arg;
    typename COMMAND_HANDSHAKE::response rsp;
    get_local_node_data(arg.node_data);
    m_payload_handler.get_payload_sync_data(arg.payload_data);
    
    simple_event ev;
    std::atomic<bool> hsh_result(false);
    
    bool r = net_utils::async_invoke_remote_command2<typename COMMAND_HANDSHAKE::response>(context_.m_connection_id, COMMAND_HANDSHAKE::ID, arg, m_net_server.get_config_object(), 
      [this, &pi, &ev, &hsh_result, &just_take_peerlist](int code, const typename COMMAND_HANDSHAKE::response& rsp, p2p_connection_context& context)
    {
      misc_utils::auto_scope_leave_caller scope_exit_handler = misc_utils::create_scope_leave_handler([&](){ev.raise();});

      if(code < 0)
      {
        LOG_PRINT_CC_RED(context, "COMMAND_HANDSHAKE invoke failed. (" << code <<  ", " << levin::get_err_descr(code) << ")", LOG_LEVEL_1);
        return;
      }

      if(rsp.node_data.network_id != BYTECOIN_NETWORK)
      {
        LOG_ERROR_CCONTEXT("COMMAND_HANDSHAKE Failed, wrong network!  (" << string_tools::get_str_from_guid_a(rsp.node_data.network_id) << "), closing connection.");
        return;
      }

      if(!handle_remote_peerlist(rsp.local_peerlist, rsp.node_data.local_time, context))
      {
        LOG_ERROR_CCONTEXT("COMMAND_HANDSHAKE: failed to handle_remote_peerlist(...), closing connection.");
        return;
      }
      hsh_result = true;
      if(!just_take_peerlist)
      {
        if(!m_payload_handler.process_payload_sync_data(rsp.payload_data, context, true))
        {
          LOG_ERROR_CCONTEXT("COMMAND_HANDSHAKE invoked, but process_payload_sync_data returned false, dropping connection.");
          hsh_result = false;
          return;
        }

        pi = context.peer_id = rsp.node_data.peer_id;
        m_peerlist.set_peer_just_seen(rsp.node_data.peer_id, context.m_remote_ip, context.m_remote_port);

        if(rsp.node_data.peer_id == m_config.m_peer_id)
        {
          LOG_PRINT_CCONTEXT_L2("Connection to self detected, dropping connection");
          hsh_result = false;
          return;
        }
        LOG_PRINT_CCONTEXT_L0(" COMMAND_HANDSHAKE INVOKED OK");
      }else
      {
        LOG_PRINT_CCONTEXT_L0(" COMMAND_HANDSHAKE(AND CLOSE) INVOKED OK");
      }
    }, P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT);

    if(r)
    {
      ev.wait();
    }

    if(!hsh_result)
    {
      LOG_PRINT_CC_L0(context_, "COMMAND_HANDSHAKE Failed");
      m_net_server.get_config_object().close(context_.m_connection_id);
    }

    return hsh_result;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::do_peer_timed_sync(const net_utils::connection_context_base& context_, peerid_type peer_id)
  {
    typename COMMAND_TIMED_SYNC::request arg = AUTO_VAL_INIT(arg);
    m_payload_handler.get_payload_sync_data(arg.payload_data);

    bool r = net_utils::async_invoke_remote_command2<typename COMMAND_TIMED_SYNC::response>(context_.m_connection_id, COMMAND_TIMED_SYNC::ID, arg, m_net_server.get_config_object(), 
      [this](int code, const typename COMMAND_TIMED_SYNC::response& rsp, p2p_connection_context& context)
    {
      if(code < 0)
      {
        LOG_PRINT_CC_RED(context, "COMMAND_TIMED_SYNC invoke failed. (" << code <<  ", " << levin::get_err_descr(code) << ")", LOG_LEVEL_1);
        return;
      }

      if(!handle_remote_peerlist(rsp.local_peerlist, rsp.local_time, context))
      {
        LOG_ERROR_CCONTEXT("COMMAND_TIMED_SYNC: failed to handle_remote_peerlist(...), closing connection.");
        m_net_server.get_config_object().close(context.m_connection_id );
      }
      if(!context.m_is_income)
        m_peerlist.set_peer_just_seen(context.peer_id, context.m_remote_ip, context.m_remote_port);
      m_payload_handler.process_payload_sync_data(rsp.payload_data, context, false);
    });

    if(!r)
    {
      LOG_PRINT_CC_L2(context_, "COMMAND_TIMED_SYNC Failed");
      return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_random_index_with_fixed_probability(size_t max_index)
  {
    //divide by zero workaround
    if(!max_index)
      return 0;

    size_t x = crypto::rand<size_t>()%(max_index+1);
    size_t res = (x*x*x)/(max_index*max_index); //parabola \/
    LOG_PRINT_L3("Random connection index=" << res << "(x="<< x << ", max_index=" << max_index << ")");
    return res;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_peer_used(const peerlist_entry& peer)
  {

    if(m_config.m_peer_id == peer.id)
      return true;//dont make connections to ourself

    bool used = false;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(cntxt.peer_id == peer.id || (!cntxt.m_is_income && peer.adr.ip == cntxt.m_remote_ip && peer.adr.port == cntxt.m_remote_port))
      {
        used = true;
        return false;//stop enumerating
      }
      return true;
    });

    return used;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_addr_connected(const net_address& peer)
  {
    bool connected = false;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(!cntxt.m_is_income && peer.ip == cntxt.m_remote_ip && peer.port == cntxt.m_remote_port)
      {
        connected = true;
        return false;//stop enumerating
      }
      return true;
    });

    return connected;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::try_to_connect_and_handshake_with_new_peer(const net_address& na, bool just_take_peerlist, uint64_t last_seen_stamp, bool white)
  {
    LOG_PRINT_L0("Connecting to " << string_tools::get_ip_string_from_int32(na.ip)  << ":" << string_tools::num_to_string_fast(na.port) << "(white=" << white << ", last_seen: " << (last_seen_stamp?misc_utils::get_time_interval_string(time(NULL) - last_seen_stamp):"never" ) << ")...");

    typename net_server::t_connection_context con = AUTO_VAL_INIT(con);
    bool res = m_net_server.connect(string_tools::get_ip_string_from_int32(na.ip),
      string_tools::num_to_string_fast(na.port),
      m_config.m_net_config.connection_timeout,
      con);
    if(!res)
    {
      LOG_PRINT_L0("Connect failed to "
        << string_tools::get_ip_string_from_int32(na.ip)
        << ":" << string_tools::num_to_string_fast(na.port)
        /*<< ", try " << try_count*/);
      //m_peerlist.set_peer_unreachable(pe);
      return false;
    }
    peerid_type pi = AUTO_VAL_INIT(pi);
    res = do_handshake_with_peer(pi, con, just_take_peerlist);
    if(!res)
    {
      LOG_PRINT_CC_L0(con, "Failed to HANDSHAKE with peer "
        << string_tools::get_ip_string_from_int32(na.ip)
        << ":" << string_tools::num_to_string_fast(na.port)
        /*<< ", try " << try_count*/);
      return false;
    }
    if(just_take_peerlist)
    {
      m_net_server.get_config_object().close(con.m_connection_id);
      LOG_PRINT_CC_GREEN(con, "CONNECTION HANDSHAKED OK AND CLOSED.", LOG_LEVEL_2);
      return true;
    }

    peerlist_entry pe_local = AUTO_VAL_INIT(pe_local);
    pe_local.adr = na;
    pe_local.id = pi;
    time(&pe_local.last_seen);
    m_peerlist.append_with_peer_white(pe_local);
    //update last seen and push it to peerlist manager

    LOG_PRINT_CC_GREEN(con, "CONNECTION HANDSHAKED OK.", LOG_LEVEL_2);
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::make_new_connection_from_peerlist(bool use_white_list)
  {
    size_t local_peers_count = use_white_list ? m_peerlist.get_white_peers_count():m_peerlist.get_gray_peers_count();
    if(!local_peers_count)
      return false;//no peers

    size_t max_random_index = std::min<uint64_t>(local_peers_count -1, 20);

    std::set<size_t> tried_peers;

    size_t try_count = 0;
    size_t rand_count = 0;
    while(rand_count < (max_random_index+1)*3 &&  try_count < 10 && !m_net_server.is_stop_signal_sent())
    {
      ++rand_count;
      size_t random_index = get_random_index_with_fixed_probability(max_random_index);
      CHECK_AND_ASSERT_MES(random_index < local_peers_count, false, "random_starter_index < peers_local.size() failed!!");

      if(tried_peers.count(random_index))
        continue;

      tried_peers.insert(random_index);
      peerlist_entry pe = AUTO_VAL_INIT(pe);
      bool r = use_white_list ? m_peerlist.get_white_peer_by_index(pe, random_index):m_peerlist.get_gray_peer_by_index(pe, random_index);
      CHECK_AND_ASSERT_MES(r, false, "Failed to get random peer from peerlist(white:" << use_white_list << ")");

      ++try_count;

      if(is_peer_used(pe))
        continue;

      LOG_PRINT_L1("Selected peer: " << pe.id << " " << string_tools::get_ip_string_from_int32(pe.adr.ip) << ":" << boost::lexical_cast<std::string>(pe.adr.port) << "[white=" << use_white_list << "] last_seen: " << (pe.last_seen ? misc_utils::get_time_interval_string(time(NULL) - pe.last_seen) : "never"));
      
      if(!try_to_connect_and_handshake_with_new_peer(pe.adr, false, pe.last_seen, use_white_list))
        continue;

      return true;
    }
    return false;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::connections_maker()
  {
    if(!m_peerlist.get_white_peers_count() && m_seed_nodes.size())
    {
      size_t try_count = 0;
      size_t current_index = crypto::rand<size_t>()%m_seed_nodes.size();
      while(true)
      {        
        if(m_net_server.is_stop_signal_sent())
          return false;

        if(try_to_connect_and_handshake_with_new_peer(m_seed_nodes[current_index], true))
          break;
        if(++try_count > m_seed_nodes.size())
        {
          LOG_PRINT_RED_L0("Failed to connect to any of seed peers, continuing without seeds");
          break;
        }
        if(++current_index >= m_seed_nodes.size())
          current_index = 0;
      }
    }

    for(const net_address& na: m_priority_peers)
    {
      if(m_net_server.is_stop_signal_sent())
        return false;

      if(is_addr_connected(na))
        continue;
      try_to_connect_and_handshake_with_new_peer(na);
    }

    size_t expected_white_connections = (m_config.m_net_config.connections_count*P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT)/100;

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
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::make_expected_connections_count(bool white_list, size_t expected_connections)
  {
    size_t conn_count = get_outgoing_connections_count();
    //add new connections from white peers
    while(conn_count < expected_connections)
    {
      if(m_net_server.is_stop_signal_sent())
        return false;

      if(!make_new_connection_from_peerlist(white_list))
        break;
      conn_count = get_outgoing_connections_count();
    }
    return true;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_outgoing_connections_count()
  {
    size_t count = 0;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(!cntxt.m_is_income)
        ++count;
      return true;
    });

    return count;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::idle_worker()
  {
    m_peer_handshake_idle_maker_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::peer_sync_idle_maker, this));
    m_connections_maker_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::connections_maker, this));
    m_peerlist_store_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::store_config, this));
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::peer_sync_idle_maker()
  {
    LOG_PRINT_L2("STARTED PEERLIST IDLE HANDSHAKE");
    typedef std::list<std::pair<net_utils::connection_context_base, peerid_type> > local_connects_type;
    local_connects_type cncts;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(cntxt.peer_id)
        cncts.push_back(local_connects_type::value_type(cntxt, cntxt.peer_id));//do idle sync only with handshaked connections
      return true;
    });

    std::for_each(cncts.begin(), cncts.end(), [&](const typename local_connects_type::value_type& vl){do_peer_timed_sync(vl.first, vl.second);});

    LOG_PRINT_L2("FINISHED PEERLIST IDLE HANDSHAKE");
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::fix_time_delta(std::list<peerlist_entry>& local_peerlist, time_t local_time, int64_t& delta)
  {
    //fix time delta
    time_t now = 0;
    time(&now);
    delta = now - local_time;

    BOOST_FOREACH(peerlist_entry& be, local_peerlist)
    {
      if(be.last_seen > local_time)
      {
        LOG_PRINT_RED_L0("FOUND FUTURE peerlist for entry " << string_tools::get_ip_string_from_int32(be.adr.ip) << ":" << be.adr.port << " last_seen: " << be.last_seen << ", local_time(on remote node):" << local_time);
        return false;
      }
      be.last_seen += delta;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::handle_remote_peerlist(const std::list<peerlist_entry>& peerlist, time_t local_time, const net_utils::connection_context_base& context)
  {
    int64_t delta = 0;
    std::list<peerlist_entry> peerlist_ = peerlist;
    if(!fix_time_delta(peerlist_, local_time, delta))
      return false;
    LOG_PRINT_CCONTEXT_L2("REMOTE PEERLIST: TIME_DELTA: " << delta << ", remote peerlist size=" << peerlist_.size());
    LOG_PRINT_CCONTEXT_L3("REMOTE PEERLIST: " <<  print_peerlist_to_string(peerlist_));
    return m_peerlist.merge_peerlist(peerlist_);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::get_local_node_data(basic_node_data& node_data)
  {
    time_t local_time;
    time(&local_time);
    node_data.local_time = local_time;
    node_data.peer_id = m_config.m_peer_id;
    if(!m_hide_my_port)
      node_data.my_port = m_external_port ? m_external_port : m_listenning_port;
    else 
      node_data.my_port = 0;
    node_data.network_id = BYTECOIN_NETWORK;
    return true;
  }
  //-----------------------------------------------------------------------------------
#ifdef ALLOW_DEBUG_COMMANDS
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::check_trust(const proof_of_trust& tr)
  {
    uint64_t local_time = time(NULL);
    uint64_t time_delata = local_time > tr.time ? local_time - tr.time: tr.time - local_time;
    if(time_delata > 24*60*60 )
    {
      LOG_ERROR("check_trust failed to check time conditions, local_time=" <<  local_time << ", proof_time=" << tr.time);
      return false;
    }
    if(m_last_stat_request_time >= tr.time )
    {
      LOG_ERROR("check_trust failed to check time conditions, last_stat_request_time=" <<  m_last_stat_request_time << ", proof_time=" << tr.time);
      return false;
    }
    if(m_config.m_peer_id != tr.peer_id)
    {
      LOG_ERROR("check_trust failed: peer_id mismatch (passed " << tr.peer_id << ", expected " << m_config.m_peer_id<< ")");
      return false;
    }
    crypto::public_key pk = AUTO_VAL_INIT(pk);
    string_tools::hex_to_pod(P2P_STAT_TRUSTED_PUB_KEY, pk);
    crypto::hash h = tools::get_proof_of_trust_hash(tr);
    if(!crypto::check_signature(h, pk, tr.sign))
    {
      LOG_ERROR("check_trust failed: sign check failed");
      return false;
    }
    //update last request time
    m_last_stat_request_time = tr.time;
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_get_stat_info(int command, typename COMMAND_REQUEST_STAT_INFO::request& arg, typename COMMAND_REQUEST_STAT_INFO::response& rsp, p2p_connection_context& context)
  {
    if(!check_trust(arg.tr))
    {
      drop_connection(context);
      return 1;
    }
    rsp.connections_count = m_net_server.get_config_object().get_connections_count();
    rsp.incoming_connections_count = rsp.connections_count - get_outgoing_connections_count();
    rsp.version = PROJECT_VERSION_LONG;
    rsp.os_version = tools::get_os_version_string();
    m_payload_handler.get_stat_info(rsp.payload_info);
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_get_network_state(int command, COMMAND_REQUEST_NETWORK_STATE::request& arg, COMMAND_REQUEST_NETWORK_STATE::response& rsp, p2p_connection_context& context)
  {
    if(!check_trust(arg.tr))
    {
      drop_connection(context);
      return 1;
    }
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      connection_entry ce;
      ce.adr.ip  = cntxt.m_remote_ip;
      ce.adr.port  = cntxt.m_remote_port;
      ce.id = cntxt.peer_id;
      ce.is_income = cntxt.m_is_income;
      rsp.connections_list.push_back(ce);
      return true;
    });

    m_peerlist.get_peerlist_full(rsp.local_peerlist_gray, rsp.local_peerlist_white);
    rsp.my_id = m_config.m_peer_id;
    rsp.local_time = time(NULL);
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_get_peer_id(int command, COMMAND_REQUEST_PEER_ID::request& arg, COMMAND_REQUEST_PEER_ID::response& rsp, p2p_connection_context& context)
  {
    rsp.my_id = m_config.m_peer_id;
    return 1;
  }
#endif
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::request_callback(const epee::net_utils::connection_context_base& context)
  {
    m_net_server.get_config_object().request_callback(context.m_connection_id);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::relay_notify_to_all(int command, const std::string& data_buff, const epee::net_utils::connection_context_base& context)
  {
    std::list<boost::uuids::uuid> connections;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(cntxt.peer_id && context.m_connection_id != cntxt.m_connection_id)
        connections.push_back(cntxt.m_connection_id);
      return true;
    });

    BOOST_FOREACH(const auto& c_id, connections)
    {
      m_net_server.get_config_object().notify(command, data_buff, c_id);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::callback(p2p_connection_context& context)
  {
    m_payload_handler.on_callback(context);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::invoke_notify_to_peer(int command, const std::string& req_buff, const epee::net_utils::connection_context_base& context)
  {
    int res = m_net_server.get_config_object().notify(command, req_buff, context.m_connection_id);
    return res > 0;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::invoke_command_to_peer(int command, const std::string& req_buff, std::string& resp_buff, const epee::net_utils::connection_context_base& context)
  {
    int res = m_net_server.get_config_object().invoke(command, req_buff, resp_buff, context.m_connection_id);
    return res > 0;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::drop_connection(const epee::net_utils::connection_context_base& context)
  {
    m_net_server.get_config_object().close(context.m_connection_id);
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler> template<class t_callback>
  bool node_server<t_payload_net_handler>::try_ping(basic_node_data& node_data, p2p_connection_context& context, t_callback cb)
  {
    if(!node_data.my_port)
      return false;

    uint32_t actual_ip =  context.m_remote_ip;
    if(!m_peerlist.is_ip_allowed(actual_ip))
      return false;
    std::string ip = string_tools::get_ip_string_from_int32(actual_ip);
    std::string port = string_tools::num_to_string_fast(node_data.my_port);
    peerid_type pr = node_data.peer_id;
    bool r = m_net_server.connect_async(ip, port, m_config.m_net_config.ping_connection_timeout, [cb, /*context,*/ ip, port, pr, this](
      const typename net_server::t_connection_context& ping_context,
      const boost::system::error_code& ec)->bool
    {
      if(ec)
      {
        LOG_PRINT_CC_L2(ping_context, "back ping connect failed to " << ip << ":" << port);
        return false;
      }
      COMMAND_PING::request req;
      COMMAND_PING::response rsp;
      //vc2010 workaround
      /*std::string ip_ = ip;
      std::string port_=port;
      peerid_type pr_ = pr;
      auto cb_ = cb;*/
      bool inv_call_res = net_utils::async_invoke_remote_command2<COMMAND_PING::response>(ping_context.m_connection_id, COMMAND_PING::ID, req, m_net_server.get_config_object(),
        [=](int code, const COMMAND_PING::response& rsp, p2p_connection_context& context)
      {
        if(code <= 0)
        {
          LOG_PRINT_CC_L2(ping_context, "Failed to invoke COMMAND_PING to " << ip << ":" << port << "(" << code <<  ", " << levin::get_err_descr(code) << ")");
          return;
        }

        if(rsp.status != PING_OK_RESPONSE_STATUS_TEXT || pr != rsp.peer_id)
        {
          LOG_PRINT_CC_L2(ping_context, "back ping invoke wrong response \"" << rsp.status << "\" from" << ip << ":" << port << ", hsh_peer_id=" << pr << ", rsp.peer_id=" << rsp.peer_id);
          return;
        }
        m_net_server.get_config_object().close(ping_context.m_connection_id);
        cb();
      });

      if(!inv_call_res)
      {
        LOG_PRINT_CC_L2(ping_context, "back ping invoke failed to " << ip << ":" << port);
        m_net_server.get_config_object().close(ping_context.m_connection_id);
        return false;
      }
      return true;
    });
    if(!r)
    {
      LOG_ERROR("Failed to call connect_async, network error.");
    }
    return r;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_timed_sync(int command, typename COMMAND_TIMED_SYNC::request& arg, typename COMMAND_TIMED_SYNC::response& rsp, p2p_connection_context& context)
  {
    if(!m_payload_handler.process_payload_sync_data(arg.payload_data, context, false))
    {
      LOG_ERROR_CCONTEXT("Failed to process_payload_sync_data(), dropping connection");
      drop_connection(context);
      return 1;
    }

    //fill response
    rsp.local_time = time(NULL);
    m_peerlist.get_peerlist_head(rsp.local_peerlist);
    m_payload_handler.get_payload_sync_data(rsp.payload_data);
    LOG_PRINT_CCONTEXT_L2("COMMAND_TIMED_SYNC");
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_handshake(int command, typename COMMAND_HANDSHAKE::request& arg, typename COMMAND_HANDSHAKE::response& rsp, p2p_connection_context& context)
  {
    if(arg.node_data.network_id != BYTECOIN_NETWORK)
    {

      LOG_PRINT_CCONTEXT_L0("WRONG NETWORK AGENT CONNECTED! id=" << string_tools::get_str_from_guid_a(arg.node_data.network_id));
      drop_connection(context);
      return 1;
    }

    if(!context.m_is_income)
    {
      LOG_ERROR_CCONTEXT("COMMAND_HANDSHAKE came not from incoming connection");
      drop_connection(context);
      return 1;
    }

    if(context.peer_id)
    {
      LOG_ERROR_CCONTEXT("COMMAND_HANDSHAKE came, but seems that connection already have associated peer_id (double COMMAND_HANDSHAKE?)");
      drop_connection(context);
      return 1;
    }

    if(!m_payload_handler.process_payload_sync_data(arg.payload_data, context, true))
    {
      LOG_ERROR_CCONTEXT("COMMAND_HANDSHAKE came, but process_payload_sync_data returned false, dropping connection.");
      drop_connection(context);
      return 1;
    }
    //associate peer_id with this connection
    context.peer_id = arg.node_data.peer_id;

    if(arg.node_data.peer_id != m_config.m_peer_id && arg.node_data.my_port)
    {
      peerid_type peer_id_l = arg.node_data.peer_id;
      uint32_t port_l = arg.node_data.my_port;
      //try ping to be sure that we can add this peer to peer_list
      try_ping(arg.node_data, context, [peer_id_l, port_l, context, this]()
      {
        //called only(!) if success pinged, update local peerlist
        peerlist_entry pe;
        pe.adr.ip = context.m_remote_ip;
        pe.adr.port = port_l;
        time(&pe.last_seen);
        pe.id = peer_id_l;
        this->m_peerlist.append_with_peer_white(pe);
        LOG_PRINT_CCONTEXT_L2("PING SUCCESS " << string_tools::get_ip_string_from_int32(context.m_remote_ip) << ":" << port_l);
      });
    }

    //fill response
    m_peerlist.get_peerlist_head(rsp.local_peerlist);
    get_local_node_data(rsp.node_data);
    m_payload_handler.get_payload_sync_data(rsp.payload_data);
    LOG_PRINT_CCONTEXT_GREEN("COMMAND_HANDSHAKE", LOG_LEVEL_1);
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_ping(int command, COMMAND_PING::request& arg, COMMAND_PING::response& rsp, p2p_connection_context& context)
  {
    LOG_PRINT_CCONTEXT_L2("COMMAND_PING");
    rsp.status = PING_OK_RESPONSE_STATUS_TEXT;
    rsp.peer_id = m_config.m_peer_id;
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::log_peerlist()
  {
    std::list<peerlist_entry> pl_wite;
    std::list<peerlist_entry> pl_gray;
    m_peerlist.get_peerlist_full(pl_gray, pl_wite);
    LOG_PRINT_L0(ENDL << "Peerlist white:" << ENDL << print_peerlist_to_string(pl_wite) << ENDL << "Peerlist gray:" << ENDL << print_peerlist_to_string(pl_gray) );
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::log_connections()
  {
    LOG_PRINT_L0("Connections: \r\n" << print_connections_container() );
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  std::string node_server<t_payload_net_handler>::print_connections_container()
  {

    std::stringstream ss;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      ss << string_tools::get_ip_string_from_int32(cntxt.m_remote_ip) << ":" << cntxt.m_remote_port
        << " \t\tpeer_id " << cntxt.peer_id
        << " \t\tconn_id " << string_tools::get_str_from_guid_a(cntxt.m_connection_id) << (cntxt.m_is_income ? " INC":" OUT")
        << std::endl;
      return true;
    });
    std::string s = ss.str();
    return s;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::on_connection_new(p2p_connection_context& context)
  {
    LOG_PRINT_L2("["<< net_utils::print_connection_context(context) << "] NEW CONNECTION");
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::on_connection_close(p2p_connection_context& context)
  {
    LOG_PRINT_L2("["<< net_utils::print_connection_context(context) << "] CLOSE CONNECTION");
  }
  //-----------------------------------------------------------------------------------
}
