// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>
#include <System/OperationTimeout.h>
#include <System/InterruptedException.h>

using namespace System;

class OperationTimeoutTest : public testing::Test {
public:
  OperationTimeoutTest() : contextGroup(dispatcher), timer(dispatcher) {
  }

  Dispatcher dispatcher;
  ContextGroup contextGroup;
  Timer timer;
};

TEST_F(OperationTimeoutTest, DISABLED_timeoutHappens) {
  OperationTimeout<Timer> op(dispatcher, timer, std::chrono::milliseconds(100));
  contextGroup.spawn([&] { 
    EXPECT_THROW(timer.sleep(std::chrono::milliseconds(200)), InterruptedException); 
  });
  contextGroup.wait();
}

TEST_F(OperationTimeoutTest, DISABLED_timeoutSkipped) {
  {
    OperationTimeout<Timer> op(dispatcher, timer, std::chrono::milliseconds(200));
    contextGroup.spawn([&] { 
      EXPECT_NO_THROW(timer.sleep(std::chrono::milliseconds(100)));
    });
    contextGroup.wait();
  }
}

TEST_F(OperationTimeoutTest, DISABLED_noOperation) {
  {
    OperationTimeout<Timer> op(dispatcher, timer, std::chrono::milliseconds(100));
  }
}
