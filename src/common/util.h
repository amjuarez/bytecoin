// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

#include <string>
#include <system_error>

#include "crypto/hash.h"


namespace nodetool {
  struct proof_of_trust;
}

namespace tools
{
  std::string get_default_data_dir();
  std::string get_os_version_string();
  bool create_directories_if_necessary(const std::string& path);
  std::error_code replace_file(const std::string& replacement_name, const std::string& replaced_name);
  crypto::hash get_proof_of_trust_hash(const nodetool::proof_of_trust& pot);
}
