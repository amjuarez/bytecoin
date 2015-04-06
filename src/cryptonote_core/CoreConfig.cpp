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

