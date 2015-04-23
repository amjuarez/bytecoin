// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CoreConfig.h"

#include "common/util.h"
#include "common/command_line.h"

namespace cryptonote {

CoreConfig::CoreConfig() {
  configFolder = tools::get_default_data_dir();
}

void CoreConfig::init(const boost::program_options::variables_map& options) {
  configFolder = command_line::get_arg(options, command_line::arg_data_dir);
}

void CoreConfig::initOptions(boost::program_options::options_description& desc) {
}
} //namespace cryptonote

