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

#include <future>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/Timer.h>
#include <gtest/gtest.h>

using namespace System;

TEST(DispatcherTests, clearRemainsDispatcherWorkable) {
  Dispatcher dispatcher;
  dispatcher.clear();
  bool spawnDone = false;
  dispatcher.spawn([&]() {
    spawnDone = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawnDone);
}

TEST(DispatcherTests, clearRemainsDispatcherWorkableAfterAsyncOperation) {
  Dispatcher dispatcher;
  bool spawn1Done = false;
  bool spawn2Done = false;
  dispatcher.spawn([&]() {
    spawn1Done = true;
  });
  
  dispatcher.yield();
  ASSERT_TRUE(spawn1Done);
  dispatcher.clear();
  dispatcher.spawn([&]() {
    spawn2Done = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawn2Done);
}

TEST(DispatcherTests, clearCalledFromSpawnRemainsDispatcherWorkable) {
  Dispatcher dispatcher;
  bool spawn1Done = false;
  bool spawn2Done = false;
  dispatcher.spawn([&]() {
    dispatcher.clear();
    spawn1Done = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawn1Done);
  dispatcher.spawn([&]() {
    spawn2Done = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawn2Done);
}

TEST(DispatcherTests, timerIsHandledOnlyAfterAllSpawnedTasksAreHandled) {
  Dispatcher dispatcher;
  Event event1(dispatcher);
  Event event2(dispatcher);
  dispatcher.spawn([&]() {
    event1.set();
    Timer(dispatcher).sleep(std::chrono::milliseconds(1));
    event2.set();
  });

  dispatcher.yield();
  ASSERT_TRUE(event1.get());
  ASSERT_FALSE(event2.get());
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  dispatcher.pushContext(dispatcher.getCurrentContext());
  dispatcher.dispatch();
  ASSERT_FALSE(event2.get());
  dispatcher.yield();
  ASSERT_TRUE(event2.get());
}

TEST(DispatcherTests, dispatchKeepsSpawnOrder) {
  Dispatcher dispatcher;
  std::deque<size_t> executionOrder;
  std::deque<size_t> expectedOrder = { 1, 2 };
  dispatcher.spawn([&]() {
    executionOrder.push_back(1);
  });

  dispatcher.spawn([&]() {
    executionOrder.push_back(2);
  });

  dispatcher.pushContext(dispatcher.getCurrentContext());
  dispatcher.dispatch();
  ASSERT_EQ(executionOrder, expectedOrder);
}

TEST(DispatcherTests, dispatchKeepsSpawnOrderWithNesting) {
  Dispatcher dispatcher;
  std::deque<size_t> executionOrder;
  std::deque<size_t> expectedOrder = { 1, 2, 3, 4 };
  auto mainContext = dispatcher.getCurrentContext();
  dispatcher.spawn([&]() {
    executionOrder.push_back(1);
    dispatcher.spawn([&]() {
      executionOrder.push_back(3);
    });
  });

  dispatcher.spawn([&]() {
    executionOrder.push_back(2);
    dispatcher.spawn([&]() {
      executionOrder.push_back(4);
      dispatcher.pushContext(mainContext);
    });
  });

  dispatcher.dispatch();
  ASSERT_EQ(executionOrder, expectedOrder);
}

TEST(DispatcherTests, dispatchKeepsSpawnResumingOrder) {
  Dispatcher dispatcher;
  std::deque<size_t> executionOrder;
  std::deque<size_t> expectedOrder = { 1, 2, 3, 4 };
  std::vector<void*> contexts;
  dispatcher.spawn([&]() {
    executionOrder.push_back(1);
    contexts.push_back(dispatcher.getCurrentContext());
    dispatcher.dispatch();
    executionOrder.push_back(3);
  });

  dispatcher.spawn([&]() {
    executionOrder.push_back(2);
    contexts.push_back(dispatcher.getCurrentContext());
    dispatcher.dispatch();
    executionOrder.push_back(4);
  });

  dispatcher.pushContext(dispatcher.getCurrentContext());
  dispatcher.dispatch();
  for (auto& ctx : contexts) {
    dispatcher.pushContext(ctx);
  }

  dispatcher.pushContext(dispatcher.getCurrentContext());
  dispatcher.dispatch();
  ASSERT_EQ(executionOrder, expectedOrder);
}

TEST(DispatcherTests, getCurrentContextDiffersForParallelSpawn) {
  Dispatcher dispatcher;
  void* ctx1 = nullptr;
  void* ctx2 = nullptr;
  dispatcher.spawn([&]() {
    ctx1 = dispatcher.getCurrentContext();
  });

  dispatcher.spawn([&]() {
    ctx2 = dispatcher.getCurrentContext();
  });

  dispatcher.yield();
  ASSERT_NE(ctx1, nullptr);
  ASSERT_NE(ctx2, nullptr);
  ASSERT_NE(ctx1, ctx2);
}

TEST(DispatcherTests, getCurrentContextSameForSequentialSpawn) {
  Dispatcher dispatcher;
  void* ctx1 = nullptr;
  void* ctx2 = nullptr;
  dispatcher.spawn([&]() {
    ctx1 = dispatcher.getCurrentContext();
    dispatcher.yield();
    ctx2 = dispatcher.getCurrentContext();
  });

  dispatcher.yield();
  dispatcher.yield();
  ASSERT_NE(ctx1, nullptr);
  ASSERT_EQ(ctx1, ctx2);
}

TEST(DispatcherTests, pushedContextMustGoOn) {
  Dispatcher dispatcher;
  bool spawnDone = false;
  dispatcher.spawn([&]() {
    spawnDone = true;
  });

  dispatcher.pushContext(dispatcher.getCurrentContext());
  dispatcher.dispatch();
  ASSERT_TRUE(spawnDone);
}

TEST(DispatcherTests, pushedContextMustGoOnFromNestedSpawns) {
  Dispatcher dispatcher;
  bool spawnDone = false;
  auto mainContext = dispatcher.getCurrentContext();
  dispatcher.spawn([&]() {
    spawnDone = true;
    dispatcher.pushContext(mainContext);
  });

  dispatcher.dispatch();
  ASSERT_TRUE(spawnDone);
}

TEST(DispatcherTests, remoteSpawnActuallySpawns) {
  Dispatcher dispatcher;
  Event remoteSpawnDone(dispatcher);
  auto remoteSpawnThread = std::thread([&] {
    dispatcher.remoteSpawn([&]() {
      remoteSpawnDone.set();
    });
  });

  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }

  dispatcher.yield();
  ASSERT_TRUE(remoteSpawnDone.get());
}

TEST(DispatcherTests, remoteSpawnActuallySpawns2) {
  Dispatcher dispatcher;
  Event remoteSpawnDone(dispatcher);
  auto remoteSpawnThread = std::thread([&] {
      dispatcher.remoteSpawn([&]() {
          remoteSpawnDone.set();
      });
  });

  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }

  Timer(dispatcher).sleep(std::chrono::milliseconds(1));
  ASSERT_TRUE(remoteSpawnDone.get());
}

TEST(DispatcherTests, remoteSpawnActuallySpawns3) {
  Dispatcher dispatcher;
  Event remoteSpawnDone(dispatcher);
  auto mainCtx = dispatcher.getCurrentContext();
  auto remoteSpawnThread = std::thread([&] {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      dispatcher.remoteSpawn([&]() {
          remoteSpawnDone.set();
          dispatcher.pushContext(mainCtx);
      });
  });

  dispatcher.dispatch();
  ASSERT_TRUE(remoteSpawnDone.get());
  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }
}

TEST(DispatcherTests, remoteSpawnSpawnsProcedureInDispatcherThread) {
  Dispatcher dispatcher;
  Event remoteSpawnDone(dispatcher);
  auto mainSpawnThrId = std::this_thread::get_id();
  decltype(mainSpawnThrId) remoteSpawnThrId;
  auto remoteSpawnThread = std::thread([&] {
    dispatcher.remoteSpawn([&]() {
      remoteSpawnThrId = std::this_thread::get_id();
      remoteSpawnDone.set();
    });
  });

  remoteSpawnDone.wait();
  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }

  ASSERT_EQ(mainSpawnThrId, remoteSpawnThrId);
}

TEST(DispatcherTests, remoteSpawnSpawnsProcedureAndKeepsOrder) {
  Dispatcher dispatcher;
  Event remoteSpawnDone(dispatcher);
  std::deque<size_t> executionOrder;
  std::deque<size_t> expectedOrder = { 1, 2 };
  auto remoteSpawnThread = std::thread([&] {
    dispatcher.remoteSpawn([&]() {
      executionOrder.push_back(1);
    });

    dispatcher.remoteSpawn([&]() {
      executionOrder.push_back(2);
      remoteSpawnDone.set();
    });
  });

  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }

  remoteSpawnDone.wait();
  ASSERT_EQ(executionOrder, expectedOrder);
}

TEST(DispatcherTests, remoteSpawnActuallyWorksParallel) {
  Dispatcher dispatcher;
  Event remoteSpawnDone(dispatcher);
  auto remoteSpawnThread = std::thread([&] {
    dispatcher.remoteSpawn([&]() {
      remoteSpawnDone.set();
    });
  });

  Timer(dispatcher).sleep(std::chrono::milliseconds(100));
  ASSERT_TRUE(remoteSpawnDone.get());

  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }
}

TEST(DispatcherTests, spawnActuallySpawns) {
  Dispatcher dispatcher;
  bool spawnDone = false;
  dispatcher.spawn([&]() {
    spawnDone = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawnDone);
}

TEST(DispatcherTests, spawnJustSpawns) {
  Dispatcher dispatcher;
  bool spawnDone = false;
  dispatcher.spawn([&]() {
    spawnDone = true;
  });

  ASSERT_FALSE(spawnDone);
  dispatcher.yield();
  ASSERT_TRUE(spawnDone);
}

TEST(DispatcherTests, yieldReturnsIfNothingToSpawn) {
  Dispatcher dispatcher;
  dispatcher.yield();
}

TEST(DispatcherTests, yieldReturnsAfterExecutionOfSpawnedProcedures) {
  Dispatcher dispatcher;
  bool spawnDone = false;
  dispatcher.spawn([&]() {
    spawnDone = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawnDone);
}

TEST(DispatcherTests, yieldReturnsAfterExecutionOfIO) {
  Dispatcher dispatcher;
  dispatcher.spawn([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    dispatcher.yield();
  });

  Timer(dispatcher).sleep(std::chrono::milliseconds(1));
  dispatcher.yield();
  SUCCEED();
}

TEST(DispatcherTests, yieldExecutesIoOnItsFront) {
  Dispatcher dispatcher;
  bool spawnDone = false;
  dispatcher.spawn([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    dispatcher.yield();
    spawnDone = true;
  });

  Timer(dispatcher).sleep(std::chrono::milliseconds(1));
  ASSERT_FALSE(spawnDone);
  dispatcher.yield();
  ASSERT_TRUE(spawnDone);
}
