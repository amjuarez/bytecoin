// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletHelper.h"
#include <boost/filesystem.hpp>

#include "string_tools.h"
#include "cryptonote_protocol/blobdatatype.h"

using namespace cryptonote;
using namespace epee;


void WalletHelper::prepareFileNames(const std::string& file_path, std::string& keys_file, std::string& wallet_file) {
  if (string_tools::get_extension(file_path) == "wallet") {
    keys_file = string_tools::cut_off_extension(file_path) + ".keys";
    wallet_file = file_path;
  } else if (string_tools::get_extension(file_path) == "keys") {
    keys_file = file_path;
    wallet_file = string_tools::cut_off_extension(file_path) + ".wallet";
  } else {
    keys_file = file_path + ".keys";
    wallet_file = file_path + ".wallet";
  }
}
