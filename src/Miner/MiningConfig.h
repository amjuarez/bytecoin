// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <string>

namespace CryptoNote {

struct MiningConfig {
  MiningConfig();

  void parse(int argc, char** argv);
  void printHelp();

  std::string miningAddress;
  std::string daemonHost;
  uint16_t daemonPort;
  size_t threadCount;
  size_t scanPeriod;
  uint8_t logLevel;
  size_t blocksLimit;
  uint64_t firstBlockTimestamp;
  int64_t blockTimestampInterval;
  bool help;
};

} //namespace CryptoNote
