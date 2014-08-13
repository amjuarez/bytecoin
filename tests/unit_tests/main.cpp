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

#include "gtest/gtest.h"

#include "include_base_utils.h"

int main(int argc, char** argv)
{
  epee::debug::get_set_enable_assert(true, false);

  //set up logging options
  epee::log_space::get_set_log_detalisation_level(true, LOG_LEVEL_0);
  epee::log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
