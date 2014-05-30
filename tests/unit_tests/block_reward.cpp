// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include "cryptonote_core/cryptonote_basic_impl.h"

using namespace cryptonote;

namespace
{
  //--------------------------------------------------------------------------------------------------------------------
  class block_reward_and_height : public ::testing::Test
  {
  protected:
    void do_test(uint64_t height, uint64_t already_generated_coins = 0)
    {
      m_block_not_too_big = get_block_reward(median_block_size, current_block_size, already_generated_coins, height, m_block_reward);
    }

    static const size_t median_block_size = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
    static const size_t current_block_size = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;

    bool m_block_not_too_big;
    uint64_t m_block_reward;
  };

  TEST_F(block_reward_and_height, calculates_correctly)
  {
    do_test(0);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, START_BLOCK_REWARD);

    do_test(1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, START_BLOCK_REWARD);

    do_test(REWARD_HALVING_INTERVAL - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, START_BLOCK_REWARD);

    do_test(REWARD_HALVING_INTERVAL);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, START_BLOCK_REWARD / 2);

    do_test(2 * REWARD_HALVING_INTERVAL - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, START_BLOCK_REWARD / 2);

    do_test(2 * REWARD_HALVING_INTERVAL);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, START_BLOCK_REWARD / 4);

    do_test(2 * REWARD_HALVING_INTERVAL);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, START_BLOCK_REWARD / 4);

    do_test(11 * REWARD_HALVING_INTERVAL - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, START_BLOCK_REWARD / 1024);

    do_test(11 * REWARD_HALVING_INTERVAL);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, START_BLOCK_REWARD / 2048);

    do_test(12 * REWARD_HALVING_INTERVAL - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, START_BLOCK_REWARD / 2048);

    do_test(12 * REWARD_HALVING_INTERVAL);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, MIN_BLOCK_REWARD);

    do_test(60 * 12 * REWARD_HALVING_INTERVAL);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, MIN_BLOCK_REWARD);

    do_test(1, MONEY_SUPPLY - MIN_BLOCK_REWARD * 2 / 3);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, MIN_BLOCK_REWARD * 2 / 3);
  }
  //--------------------------------------------------------------------------------------------------------------------
  class block_reward_and_current_block_size : public ::testing::Test
  {
  protected:
    virtual void SetUp()
    {
      m_block_not_too_big = get_block_reward(0, 0, already_generated_coins, height, m_standard_block_reward);
      ASSERT_TRUE(m_block_not_too_big);
      ASSERT_LT(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE, m_standard_block_reward);
    }

    void do_test(size_t median_block_size, size_t current_block_size)
    {
      m_block_not_too_big = get_block_reward(median_block_size, current_block_size, already_generated_coins, height, m_block_reward);
    }

    static const uint64_t already_generated_coins = 0;
    static const uint64_t height = 1;

    bool m_block_not_too_big;
    uint64_t m_block_reward;
    uint64_t m_standard_block_reward;
  };

  TEST_F(block_reward_and_current_block_size, handles_block_size_less_relevance_level)
  {
    do_test(0, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_eq_relevance_level)
  {
    do_test(0, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_gt_relevance_level)
  {
    do_test(0, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE + 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_LT(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_less_2_relevance_level)
  {
    do_test(0, 2 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_LT(m_block_reward, m_standard_block_reward);
    ASSERT_LT(0, m_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_eq_2_relevance_level)
  {
    do_test(0, 2 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(0, m_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_gt_2_relevance_level)
  {
    do_test(0, 2 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE + 1);
    ASSERT_FALSE(m_block_not_too_big);
  }

  TEST_F(block_reward_and_current_block_size, fails_on_huge_median_size)
  {
#if !defined(NDEBUG)
    size_t huge_size = std::numeric_limits<uint32_t>::max() + UINT64_C(2);
    ASSERT_DEATH(do_test(huge_size, huge_size + 1), "");
#endif
  }

  TEST_F(block_reward_and_current_block_size, fails_on_huge_block_size)
  {
#if !defined(NDEBUG)
    size_t huge_size = std::numeric_limits<uint32_t>::max() + UINT64_C(2);
    ASSERT_DEATH(do_test(huge_size - 2, huge_size), "");
#endif
  }

  //--------------------------------------------------------------------------------------------------------------------
  class block_reward_and_last_block_sizes : public ::testing::Test
  {
  protected:
    virtual void SetUp()
    {
      m_last_block_sizes.push_back(3  * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(5  * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(7  * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(11 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(13 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);

      m_last_block_sizes_median = 7 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;

      m_block_not_too_big = get_block_reward(epee::misc_utils::median(m_last_block_sizes), 0, already_generated_coins, height, m_standard_block_reward);
      ASSERT_TRUE(m_block_not_too_big);
      ASSERT_LT(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE, m_standard_block_reward);
    }

    void do_test(size_t current_block_size)
    {
      m_block_not_too_big = get_block_reward(epee::misc_utils::median(m_last_block_sizes), current_block_size, already_generated_coins, height, m_block_reward);
    }

    static const uint64_t already_generated_coins = 0;
    static const uint64_t height = 1;

    std::vector<size_t> m_last_block_sizes;
    uint64_t m_last_block_sizes_median;
    bool m_block_not_too_big;
    uint64_t m_block_reward;
    uint64_t m_standard_block_reward;
  };

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_less_median)
  {
    do_test(m_last_block_sizes_median - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_eq_median)
  {
    do_test(m_last_block_sizes_median);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_gt_median)
  {
    do_test(m_last_block_sizes_median + 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_LT(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_less_2_medians)
  {
    do_test(2 * m_last_block_sizes_median - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_LT(m_block_reward, m_standard_block_reward);
    ASSERT_LT(0, m_block_reward);
  }

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_eq_2_medians)
  {
    do_test(2 * m_last_block_sizes_median);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(0, m_block_reward);
  }

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_gt_2_medians)
  {
    do_test(2 * m_last_block_sizes_median + 1);
    ASSERT_FALSE(m_block_not_too_big);
  }

  TEST_F(block_reward_and_last_block_sizes, calculates_correctly)
  {
    ASSERT_EQ(0, m_last_block_sizes_median % 8);

    do_test(m_last_block_sizes_median * 9 / 8);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward * 63 / 64);

    // 3/2 = 12/8
    do_test(m_last_block_sizes_median * 3 / 2);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward * 3 / 4);

    do_test(m_last_block_sizes_median * 15 / 8);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward * 15 / 64);
  }
}
