// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
