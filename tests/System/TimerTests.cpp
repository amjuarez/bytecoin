// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include <thread>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/InterruptedException.h>
#include <System/Timer.h>
#include <gtest/gtest.h>

using namespace System;

TEST(TimerTests, timerIsWorking) {
  Dispatcher dispatcher;
  bool done = false;
  dispatcher.spawn([&]() {
    done = true;
  });

  ASSERT_FALSE(done);
  Timer(dispatcher).sleep(std::chrono::milliseconds(10));
  ASSERT_TRUE(done);
}

TEST(TimerTests, movedTimerIsWorking) {
  Dispatcher dispatcher;
  Timer t(std::move(Timer(dispatcher)));
  bool done = false;
  dispatcher.spawn([&]() {
    done = true;
  });

  ASSERT_FALSE(done);
  t.sleep(std::chrono::milliseconds(10));
  ASSERT_TRUE(done);
}

TEST(TimerTests, movedAndStoopedTimerIsWorking) {
  Dispatcher dispatcher;
  Timer src(dispatcher);
  src.stop();
  Timer t(std::move(src));
  
  ASSERT_ANY_THROW(t.sleep(std::chrono::milliseconds(1)));
}

TEST(TimerTests, movedTimerIsWorking2) {
  Dispatcher dispatcher;
  Timer t(dispatcher);
  t = std::move(Timer(dispatcher));
  bool done = false;
  dispatcher.spawn([&]() {
    done = true;
  });

  ASSERT_FALSE(done);
  t.sleep(std::chrono::milliseconds(10));
  ASSERT_TRUE(done);
}

TEST(TimerTests, movedAndStoopedTimerIsWorking2) {
  Dispatcher dispatcher;
  Timer src(dispatcher);
  src.stop();
  Timer t(dispatcher);
  t = std::move(src);

  ASSERT_ANY_THROW(t.sleep(std::chrono::milliseconds(1)));
}

TEST(TimerTests, movedTimerIsTheSame) {
  Dispatcher dispatcher;
  Timer timer(dispatcher);
  auto timerPtr1 = &timer;
  Timer srcEvent(dispatcher);
  timer = std::move(srcEvent);
  auto timerPtr2 = &timer;
  ASSERT_EQ(timerPtr1, timerPtr2);
}

TEST(TimerTests, timerStartIsWorking) {
  Dispatcher dispatcher;
  Timer t(dispatcher);
  t.stop();
  ASSERT_ANY_THROW(t.sleep(std::chrono::milliseconds(1)));
  t.start();
  ASSERT_NO_THROW(t.sleep(std::chrono::milliseconds(1)));
}

TEST(TimerTests, timerStopBeforeSleep) {
  Dispatcher dispatcher;
  Timer t(dispatcher);
  t.stop();
  ASSERT_THROW(t.sleep(std::chrono::milliseconds(1)), InterruptedException);
  ASSERT_THROW(t.sleep(std::chrono::milliseconds(1)), InterruptedException);
}

TEST(TimerTests, timerIsCancelable) {
  Dispatcher dispatcher;
  Timer t(dispatcher);
  dispatcher.spawn([&]() {
    t.stop();
  });

  ASSERT_THROW(t.sleep(std::chrono::milliseconds(100)), InterruptedException);
}

TEST(TimerTests, DISABLED_sleepThrowsOnlyIfTimerIsStoppedBeforeTime) {
  Dispatcher dispatcher;
  Timer t(dispatcher);
  dispatcher.spawn([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    t.stop();
  });

  ASSERT_NO_THROW(t.sleep(std::chrono::milliseconds(1)));
  ASSERT_THROW(t.sleep(std::chrono::milliseconds(1)), InterruptedException);
}

TEST(TimerTests, sleepIsSleepingAtLeastTakenTime) {
  Dispatcher dispatcher;
  Timer t(dispatcher);
  auto timepoint1 = std::chrono::high_resolution_clock::now();
  t.sleep(std::chrono::milliseconds(100));
  auto timepoint2 = std::chrono::high_resolution_clock::now();
  
  ASSERT_LE(100, std::chrono::duration_cast<std::chrono::milliseconds>(timepoint2 - timepoint1).count());
}

TEST(TimerTests, timerIsReusable) {
  Dispatcher dispatcher;
  Timer t(dispatcher);
  ASSERT_NO_THROW(t.sleep(std::chrono::milliseconds(1)));
  ASSERT_NO_THROW(t.sleep(std::chrono::milliseconds(1)));
}

TEST(TimerTests, timerWithZeroTimeIsYielding) {
  Dispatcher dispatcher;
  bool done = false;
  dispatcher.spawn([&]() {
    done = true;
  });

  ASSERT_FALSE(done);
  Timer(dispatcher).sleep(std::chrono::milliseconds(0));
  ASSERT_TRUE(done);
}
