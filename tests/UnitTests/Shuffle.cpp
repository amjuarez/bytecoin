// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string>

#include "gtest/gtest.h"

#include <unordered_set>
#include "crypto/crypto.h"
#include "Common/ShuffleGenerator.h"

class ShuffleTest : public ::testing::Test {
public:

  typedef ShuffleGenerator<size_t, std::default_random_engine> DefaultShuffleGenerator;
  typedef ShuffleGenerator<size_t, Crypto::random_engine<size_t>> CryptoShuffleGenerator;

  template <typename Gen>
  void checkUniqueness(Gen& gen, size_t count) {

    std::unordered_set<size_t> values;

    for (auto i = 0; i < count; ++i) {
      auto value = gen();
      bool inserted = values.insert(value).second;
      EXPECT_TRUE(inserted);
    }
  }

  template <typename Gen>
  void consume(Gen& gen, size_t count) {
    for (auto i = 0; i < count; ++i) {
      gen();
    }
  }

  template <typename ShuffleT>
  void checkEngine(size_t N, size_t consumeCount, bool check) {
    ShuffleT gen(N);
    check ? checkUniqueness(gen, consumeCount) : consume(gen, consumeCount);
  }

};


namespace {
const size_t ITERATIONS = 10000;
}

TEST_F(ShuffleTest, correctness) {
  checkEngine<DefaultShuffleGenerator>(ITERATIONS, ITERATIONS, true);
}

TEST_F(ShuffleTest, correctness_fractionalSize) {
  checkEngine<DefaultShuffleGenerator>(ITERATIONS, ITERATIONS, true);
  checkEngine<DefaultShuffleGenerator>(ITERATIONS, ITERATIONS/2, true);
  checkEngine<DefaultShuffleGenerator>(ITERATIONS, ITERATIONS/3, true);
}


TEST_F(ShuffleTest, cryptoGenerator) {
  checkEngine<CryptoShuffleGenerator>(ITERATIONS * 3, ITERATIONS, false);
}
