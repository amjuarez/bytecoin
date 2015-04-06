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

#include <common/util.h>
#include "NetNodeConfig.h"

#include "cryptonote_config.h"
#include "common/command_line.h"

namespace nodetool {
namespace {

const command_line::arg_descriptor<std::string> arg_p2p_bind_ip        = {"p2p-bind-ip", "Interface for p2p network protocol", "0.0.0.0"};
const command_line::arg_descriptor<std::string> arg_p2p_bind_port      = {"p2p-bind-port", "Port for p2p network protocol", std::to_string(cryptonote::P2P_DEFAULT_PORT)};
const command_line::arg_descriptor<uint32_t>    arg_p2p_external_port  = {"p2p-external-port", "External port for p2p network protocol (if port forwarding used with NAT)", 0};
const command_line::arg_descriptor<bool>        arg_p2p_allow_local_ip = {"allow-local-ip", "Allow local ip add to peer list, mostly in debug purposes"};
const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_peer   = {"add-peer", "Manually add peer to local peerlist"};
const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_priority_node   = {"add-priority-node", "Specify list of peers to connect to and attempt to keep the connection open"};
const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_exclusive_node   = {"add-exclusive-node", "Specify list of peers to connect to only."
      " If this option is given the options add-priority-node and seed-node are ignored"};
const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_seed_node   = {"seed-node", "Connect to a node to retrieve peer addresses, and disconnect"};
const command_line::arg_descriptor<bool> arg_p2p_hide_my_port   =    {"hide-my-port", "Do not announce yourself as peerlist candidate", false, true};

bool parsePeerFromString(nodetool::net_address& pe, const std::string& node_addr) {
  return epee::string_tools::parse_peer_from_string(pe.ip, pe.port, node_addr);
}

bool parsePeersAndAddToContainer(const boost::program_options::variables_map& vm,
    const command_line::arg_descriptor<std::vector<std::string>>& arg,
    std::vector<nodetool::net_address>& container)
{
  std::vector<std::string> peers = command_line::get_arg(vm, arg);

  for(const std::string& str: peers) {
    nodetool::net_address na = boost::value_initialized<nodetool::net_address>();
    if (!parsePeerFromString(na, str)) {
      return false;
    }
    container.push_back(na);
  }

  return true;
}

} //namespace

void NetNodeConfig::initOptions(boost::program_options::options_description& desc) {
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

NetNodeConfig::NetNodeConfig() {
  bindIp = "0.0.0.0";
  bindPort = std::to_string(cryptonote::P2P_DEFAULT_PORT);
  externalPort = 0;
  allowLocalIp = false;
  hideMyPort = false;
  configFolder = tools::get_default_data_dir();
}

bool NetNodeConfig::init(const boost::program_options::variables_map& vm)
{
  bindIp = command_line::get_arg(vm, arg_p2p_bind_ip);
  bindPort = command_line::get_arg(vm, arg_p2p_bind_port);
  externalPort = command_line::get_arg(vm, arg_p2p_external_port);
  allowLocalIp = command_line::get_arg(vm, arg_p2p_allow_local_ip);
  configFolder = command_line::get_arg(vm, command_line::arg_data_dir);

  if (command_line::has_arg(vm, arg_p2p_add_peer))
  {
    std::vector<std::string> perrs = command_line::get_arg(vm, arg_p2p_add_peer);
    for(const std::string& pr_str: perrs)
    {
      nodetool::peerlist_entry pe = boost::value_initialized<nodetool::peerlist_entry>();
      pe.id = crypto::rand<uint64_t>();
      if (!parsePeerFromString(pe.adr, pr_str)) {
        return false;
      }
      peers.push_back(pe);
    }
  }

  if (command_line::has_arg(vm,arg_p2p_add_exclusive_node)) {
    if (!parsePeersAndAddToContainer(vm, arg_p2p_add_exclusive_node, exclusiveNodes))
      return false;
  }

  if (command_line::has_arg(vm, arg_p2p_add_priority_node)) {
    if (!parsePeersAndAddToContainer(vm, arg_p2p_add_priority_node, priorityNodes))
      return false;
  }

  if (command_line::has_arg(vm, arg_p2p_seed_node)) {
    if (!parsePeersAndAddToContainer(vm, arg_p2p_seed_node, seedNodes))
      return false;
  }

  if(command_line::has_arg(vm, arg_p2p_hide_my_port))
    hideMyPort = true;

  return true;
}

} //namespace nodetool

