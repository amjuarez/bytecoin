// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "../integration_test_lib/BaseFunctionalTest.h"
#include "../integration_test_lib/Logger.h"
#include "gtest/gtest.h"

extern System::Dispatcher globalSystem;
extern cryptonote::Currency currency;
extern Tests::Common::BaseFunctionalTestConfig config;

class TransfersTest :
  public Tests::Common::BaseFunctionalTest,
  public ::testing::Test {

public:
  TransfersTest() : BaseFunctionalTest(currency, globalSystem, config) {
  }
};

