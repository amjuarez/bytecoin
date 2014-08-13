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
