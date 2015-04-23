// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include <boost/program_options.hpp>
#include "p2p_protocol_defs.h"

namespace nodetool {

class NetNodeConfig {
public:
  NetNodeConfig();
  static void initOptions(boost::program_options::options_description& desc);
  bool init(const boost::program_options::variables_map& vm);

  std::string bindIp;
  std::string bindPort;
  uint32_t externalPort;
  bool allowLocalIp;
  std::vector<peerlist_entry> peers;
  std::vector<net_address> priorityNodes;
  std::vector<net_address> exclusiveNodes;
  std::vector<net_address> seedNodes;
  bool hideMyPort;
  std::string configFolder;
};

} //namespace nodetool
