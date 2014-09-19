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

#include <condition_variable>
#include <chrono>
#include <mutex>
#include <thread>

#include "gtest/gtest.h"

#include "include_base_utils.h"
#include "string_tools.h"
#include "net/abstract_tcp_server2.h"

namespace
{
  const uint32_t test_server_port = 5626;
  const std::string test_server_host("127.0.0.1");

  struct test_connection_context : public epee::net_utils::connection_context_base
  {
  };

  struct test_protocol_handler_config
  {
  };

  struct test_protocol_handler
  {
    typedef test_connection_context connection_context;
    typedef test_protocol_handler_config config_type;

    test_protocol_handler(epee::net_utils::i_service_endpoint* /*psnd_hndlr*/, config_type& /*config*/, connection_context& /*conn_context*/)
    {
    }

    void after_init_connection()
    {
    }

    void handle_qued_callback()
    {
    }

    bool release_protocol()
    {
      return true;
    }

    bool handle_recv(const void* /*data*/, size_t /*size*/)
    {
      return false;
    }
  };

  typedef epee::net_utils::boosted_tcp_server<test_protocol_handler> test_tcp_server;
}

TEST(boosted_tcp_server, worker_threads_are_exception_resistant)
{
  test_tcp_server srv;
  ASSERT_TRUE(srv.init_server(test_server_port, test_server_host));

  std::mutex mtx;
  std::condition_variable cond;
  int counter = 0;

  auto counter_incrementer = [&counter, &cond, &mtx]()
  {
    std::unique_lock<std::mutex> lock(mtx);
    ++counter;
    if (4 <= counter)
    {
      cond.notify_one();
    }
  };

  // 2 theads, but 4 exceptions
  ASSERT_TRUE(srv.run_server(2, false));
  ASSERT_TRUE(srv.async_call([&counter_incrementer]() { counter_incrementer(); throw std::runtime_error("test 1"); }));
  ASSERT_TRUE(srv.async_call([&counter_incrementer]() { counter_incrementer(); throw std::string("test 2"); }));
  ASSERT_TRUE(srv.async_call([&counter_incrementer]() { counter_incrementer(); throw "test 3"; }));
  ASSERT_TRUE(srv.async_call([&counter_incrementer]() { counter_incrementer(); throw 4; }));

  {
    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_NE(std::cv_status::timeout, cond.wait_for(lock, std::chrono::seconds(5)));
    ASSERT_EQ(4, counter);
  }

  // Check if threads are alive
  counter = 0;
  //auto counter_incrementer = [&counter]() { counter.fetch_add(1); epee::misc_utils::sleep_no_w(counter.load() * 10); };
  ASSERT_TRUE(srv.async_call(counter_incrementer));
  ASSERT_TRUE(srv.async_call(counter_incrementer));
  ASSERT_TRUE(srv.async_call(counter_incrementer));
  ASSERT_TRUE(srv.async_call(counter_incrementer));

  {
    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_NE(std::cv_status::timeout, cond.wait_for(lock, std::chrono::seconds(5)));
    ASSERT_EQ(4, counter);
  }

  srv.send_stop_signal();
  ASSERT_TRUE(srv.timed_wait_server_stop(5 * 1000));
  ASSERT_TRUE(srv.deinit_server());
}
