// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include "Common/Math.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/Currency.h"
#include "Logging/ConsoleLogger.h"

using namespace CryptoNote;

namespace
{
  const size_t TEST_GRANTED_FULL_REWARD_ZONE = 32000;

  //--------------------------------------------------------------------------------------------------------------------
  class block_reward_and_height : public ::testing::Test
  {
  protected:

    block_reward_and_height() : 
      m_currency(CurrencyBuilder(m_logger).currency()) {}

    void do_test(uint32_t height, uint64_t already_generated_coins = 0)
    {
      int64_t emissionChange;
      m_block_not_too_big = m_currency.getBlockReward(median_block_size, current_block_size, 
        already_generated_coins, 0, height, m_block_reward, emissionChange);
    }

    static const size_t median_block_size = TEST_GRANTED_FULL_REWARD_ZONE;
    static const size_t current_block_size = TEST_GRANTED_FULL_REWARD_ZONE;

    Logging::ConsoleLogger m_logger;
    bool m_block_not_too_big;
    uint64_t m_block_reward;
    Currency m_currency;
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

    do_test(1, parameters::MONEY_SUPPLY - MIN_BLOCK_REWARD * 2 / 3);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, MIN_BLOCK_REWARD * 2 / 3);
  }
  //--------------------------------------------------------------------------------------------------------------------
  class block_reward_and_current_block_size : public ::testing::Test
  {
  protected:

    block_reward_and_current_block_size() :
      m_currency(CurrencyBuilder(m_logger).
      blockGrantedFullRewardZone(TEST_GRANTED_FULL_REWARD_ZONE).
      currency()) {
    }

    virtual void SetUp()
    {
      int64_t emissionChange;
      m_block_not_too_big = m_currency.getBlockReward(0, 0, 
        already_generated_coins, 0, height, m_standard_block_reward, emissionChange);
      ASSERT_TRUE(m_block_not_too_big);
      ASSERT_LT(TEST_GRANTED_FULL_REWARD_ZONE, m_standard_block_reward);
    }

    void do_test(size_t median_block_size, size_t current_block_size)
    {
      int64_t emissionChange;
      m_block_not_too_big = m_currency.getBlockReward(median_block_size, current_block_size, 
        already_generated_coins, 0, height, m_block_reward, emissionChange);
    }

    static const uint64_t already_generated_coins = 0;
    static const uint32_t height = 1;

    Logging::ConsoleLogger m_logger;
    bool m_block_not_too_big;
    uint64_t m_block_reward;
    uint64_t m_standard_block_reward;
    Currency m_currency;
  };

  TEST_F(block_reward_and_current_block_size, handles_block_size_less_relevance_level)
  {
    do_test(0, TEST_GRANTED_FULL_REWARD_ZONE - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_eq_relevance_level)
  {
    do_test(0, TEST_GRANTED_FULL_REWARD_ZONE);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_gt_relevance_level)
  {
    do_test(0, TEST_GRANTED_FULL_REWARD_ZONE + 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_LT(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_less_2_relevance_level)
  {
    do_test(0, 2 * TEST_GRANTED_FULL_REWARD_ZONE - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_LT(m_block_reward, m_standard_block_reward);
    ASSERT_LT(0, m_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_eq_2_relevance_level)
  {
    do_test(0, 2 * TEST_GRANTED_FULL_REWARD_ZONE);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(0, m_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_gt_2_relevance_level)
  {
    do_test(0, 2 * TEST_GRANTED_FULL_REWARD_ZONE + 1);
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

    block_reward_and_last_block_sizes() :
      m_currency(CurrencyBuilder(m_logger).currency()) {}

    virtual void SetUp()
    {
      int64_t emissionChange;

      m_last_block_sizes.push_back(3  * TEST_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(5  * TEST_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(7  * TEST_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(11 * TEST_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(13 * TEST_GRANTED_FULL_REWARD_ZONE);

      m_last_block_sizes_median = 7 * TEST_GRANTED_FULL_REWARD_ZONE;

      m_block_not_too_big = m_currency.getBlockReward(Common::medianValue(m_last_block_sizes), 0,
        already_generated_coins, 0, height, m_standard_block_reward, emissionChange);

      ASSERT_TRUE(m_block_not_too_big);
      ASSERT_LT(TEST_GRANTED_FULL_REWARD_ZONE, m_standard_block_reward);
    }

    void do_test(size_t current_block_size)
    {
      int64_t emissionChange;
      m_block_not_too_big = m_currency.getBlockReward(Common::medianValue(m_last_block_sizes), current_block_size,
        already_generated_coins, 0, height, m_block_reward, emissionChange);
    }

    static const uint64_t already_generated_coins = 0;
    static const uint32_t height = 1;

    Logging::ConsoleLogger m_logger;
    std::vector<size_t> m_last_block_sizes;
    uint64_t m_last_block_sizes_median;
    bool m_block_not_too_big;
    uint64_t m_block_reward;
    uint64_t m_standard_block_reward;
    Currency m_currency;
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
