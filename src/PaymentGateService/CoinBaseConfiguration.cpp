// Copyright (c) 2015-2016, The Forknote developers
//
// This file is part of Forknote.
//
// Forknote is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Forknote is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Forknote.  If not, see <http://www.gnu.org/licenses/>.

// Copyright (c) 2016, The Forknote developers
//
// This file is part of Forknote.
//
// Forknote is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Forknote is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Forknote.  If not, see <http://www.gnu.org/licenses/>.

#include "CoinBaseConfiguration.h"

namespace PaymentService {

namespace po = boost::program_options;

CoinBaseConfiguration::CoinBaseConfiguration() {
    CRYPTONOTE_NAME=CryptoNote::CRYPTONOTE_NAME;
    GENESIS_COINBASE_TX_HEX=CryptoNote::parameters::GENESIS_COINBASE_TX_HEX;
    CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX=CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
    MONEY_SUPPLY=CryptoNote::parameters::MONEY_SUPPLY;
    BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX=CryptoNote::parameters::BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX;
    ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX=CryptoNote::parameters::ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX;
    ZAWY_LWMA_DIFFICULTY_LAST_BLOCK=CryptoNote::parameters::ZAWY_LWMA_DIFFICULTY_LAST_BLOCK;
    ZAWY_LWMA_DIFFICULTY_N=CryptoNote::parameters::ZAWY_LWMA_DIFFICULTY_N;
    ZAWY_DIFFICULTY_BLOCK_INDEX=CryptoNote::parameters::ZAWY_DIFFICULTY_BLOCK_INDEX;
    ZAWY_DIFFICULTY_LAST_BLOCK=CryptoNote::parameters::ZAWY_DIFFICULTY_LAST_BLOCK;
    GENESIS_BLOCK_REWARD=CryptoNote::parameters::GENESIS_BLOCK_REWARD;
    CRYPTONOTE_COIN_VERSION=CryptoNote::parameters::CRYPTONOTE_COIN_VERSION;
    TAIL_EMISSION_REWARD=CryptoNote::parameters::TAIL_EMISSION_REWARD;
    KILL_HEIGHT=CryptoNote::parameters::KILL_HEIGHT;
    MANDATORY_TRANSACTION=CryptoNote::parameters::MANDATORY_TRANSACTION;
    MIXIN_START_HEIGHT=CryptoNote::parameters::MIXIN_START_HEIGHT;
    MIN_MIXIN=CryptoNote::parameters::MIN_MIXIN;
    MANDATORY_MIXIN_BLOCK_VERSION=CryptoNote::parameters::MANDATORY_MIXIN_BLOCK_VERSION;
    EMISSION_SPEED_FACTOR=CryptoNote::parameters::EMISSION_SPEED_FACTOR;
    CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE=CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
    CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1=CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1;
    CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2=CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2;
    CRYPTONOTE_DISPLAY_DECIMAL_POINT=CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT;
    MINIMUM_FEE=CryptoNote::parameters::MINIMUM_FEE;
    DEFAULT_DUST_THRESHOLD=CryptoNote::parameters::DEFAULT_DUST_THRESHOLD;
    DIFFICULTY_TARGET=CryptoNote::parameters::DIFFICULTY_TARGET;
    CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW=CryptoNote::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;
    MAX_BLOCK_SIZE_INITIAL=CryptoNote::parameters::MAX_BLOCK_SIZE_INITIAL;
    EXPECTED_NUMBER_OF_BLOCKS_PER_DAY=0;
    UPGRADE_HEIGHT_V2=0;
    UPGRADE_HEIGHT_V3=0;
    KEY_IMAGE_CHECKING_BLOCK_INDEX=0;
    DIFFICULTY_WINDOW=CryptoNote::parameters::DIFFICULTY_WINDOW;
    DIFFICULTY_WINDOW_V1=CryptoNote::parameters::DIFFICULTY_WINDOW_V1;
    DIFFICULTY_WINDOW_V2=CryptoNote::parameters::DIFFICULTY_WINDOW_V2;
    DIFFICULTY_LAG_V1=CryptoNote::parameters::DIFFICULTY_LAG_V1;
    DIFFICULTY_LAG_V2=CryptoNote::parameters::DIFFICULTY_LAG_V2;
    DIFFICULTY_CUT_V1=CryptoNote::parameters::DIFFICULTY_CUT_V1;
    DIFFICULTY_CUT_V2=CryptoNote::parameters::DIFFICULTY_CUT_V2;
MAX_TRANSACTION_SIZE_LIMIT=CryptoNote::parameters::MAX_TRANSACTION_SIZE_LIMIT;
    DIFFICULTY_CUT=CryptoNote::parameters::DIFFICULTY_CUT;
    DIFFICULTY_LAG=CryptoNote::parameters::DIFFICULTY_LAG;
}

void CoinBaseConfiguration::initOptions(boost::program_options::options_description& desc) {
  desc.add_options()
    ("CRYPTONOTE_NAME", po::value<std::string>()->default_value(CryptoNote::CRYPTONOTE_NAME), "Blockchain name")
    ("GENESIS_COINBASE_TX_HEX", po::value<std::string>()->default_value(CryptoNote::parameters::GENESIS_COINBASE_TX_HEX), "Genesis transaction hex")
    ("CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX", po::value<uint64_t>()->default_value(CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX), "uint64_t")
    ("MONEY_SUPPLY", po::value<uint64_t>()->default_value(CryptoNote::parameters::MONEY_SUPPLY), "uint64_t")
    ("BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("ZAWY_LWMA_DIFFICULTY_LAST_BLOCK", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("ZAWY_LWMA_DIFFICULTY_N", po::value<size_t>()->default_value(0), "size_t")
    ("ZAWY_DIFFICULTY_BLOCK_INDEX", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("ZAWY_DIFFICULTY_LAST_BLOCK", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("GENESIS_BLOCK_REWARD", po::value<uint64_t>()->default_value(0), "uint64_t")
    ("CRYPTONOTE_COIN_VERSION", po::value<size_t>()->default_value(0), "size_t")
    ("TAIL_EMISSION_REWARD", po::value<uint64_t>()->default_value(0), "uint64_t")
    ("KILL_HEIGHT", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("MANDATORY_TRANSACTION", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("MIXIN_START_HEIGHT", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("MIN_MIXIN", po::value<uint16_t>()->default_value(0), "uint16_t")
    ("MANDATORY_MIXIN_BLOCK_VERSION", po::value<uint8_t>()->default_value(0), "uint8_t")
    ("EMISSION_SPEED_FACTOR", po::value<unsigned>()->default_value(CryptoNote::parameters::EMISSION_SPEED_FACTOR), "unsigned")
    ("CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE", po::value<size_t>()->default_value(CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE), "size_t")
    ("CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1", po::value<size_t>()->default_value(CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1), "size_t")
    ("CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2", po::value<size_t>()->default_value(CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2), "size_t")
    ("CRYPTONOTE_DISPLAY_DECIMAL_POINT", po::value<size_t>()->default_value(CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT), "size_t")
    ("MINIMUM_FEE", po::value<uint64_t>()->default_value(CryptoNote::parameters::MINIMUM_FEE), "uint64_t")
    ("DEFAULT_DUST_THRESHOLD", po::value<uint64_t>()->default_value(CryptoNote::parameters::DEFAULT_DUST_THRESHOLD), "uint64_t")
    ("DIFFICULTY_TARGET", po::value<uint64_t>()->default_value(CryptoNote::parameters::DIFFICULTY_TARGET), "uint64_t")
    ("CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW", po::value<uint32_t>()->default_value(CryptoNote::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW), "uint32_t")
    ("MAX_BLOCK_SIZE_INITIAL", po::value<size_t>()->default_value(CryptoNote::parameters::MAX_BLOCK_SIZE_INITIAL), "size_t")
    ("EXPECTED_NUMBER_OF_BLOCKS_PER_DAY", po::value<uint64_t>()->default_value(0), "uint64_t")
    ("UPGRADE_HEIGHT_V2", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("UPGRADE_HEIGHT_V3", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("KEY_IMAGE_CHECKING_BLOCK_INDEX", po::value<uint32_t>()->default_value(0), "uint32_t")
    ("DIFFICULTY_WINDOW", po::value<size_t>()->default_value(0), "size_t")
    ("DIFFICULTY_WINDOW_V1", po::value<size_t>()->default_value(0), "size_t")
    ("DIFFICULTY_WINDOW_V2", po::value<size_t>()->default_value(0), "size_t")
    ("DIFFICULTY_CUT_V1", po::value<size_t>()->default_value(CryptoNote::parameters::DIFFICULTY_CUT_V1), "size_t")
    ("DIFFICULTY_CUT_V2", po::value<size_t>()->default_value(CryptoNote::parameters::DIFFICULTY_CUT_V2), "size_t")
    ("DIFFICULTY_LAG_V1", po::value<size_t>()->default_value(CryptoNote::parameters::DIFFICULTY_LAG_V1), "size_t")
    ("DIFFICULTY_LAG_V2", po::value<size_t>()->default_value(CryptoNote::parameters::DIFFICULTY_LAG_V2), "size_t")
("MAX_TRANSACTION_SIZE_LIMIT", po::value<uint64_t>()->default_value(0), "uint64_t")
    ("DIFFICULTY_CUT", po::value<size_t>()->default_value(CryptoNote::parameters::DIFFICULTY_CUT), "size_t")
    ("DIFFICULTY_LAG", po::value<size_t>()->default_value(CryptoNote::parameters::DIFFICULTY_LAG), "size_t")
    ;

}

void CoinBaseConfiguration::init(const boost::program_options::variables_map& options) {
  if (options.count("CRYPTONOTE_NAME")) {
    CRYPTONOTE_NAME = options["CRYPTONOTE_NAME"].as<std::string>();
  }
  if (options.count("GENESIS_COINBASE_TX_HEX")) {
    GENESIS_COINBASE_TX_HEX = options["GENESIS_COINBASE_TX_HEX"].as<std::string>();
  }
  if (options.count("CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX")) {
    CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = options["CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX"].as<uint64_t>();
  }
  if (options.count("MONEY_SUPPLY")) {
    MONEY_SUPPLY = options["MONEY_SUPPLY"].as<uint64_t>();
  }
  if (options.count("MIN_MIXIN")) {
    MIN_MIXIN = options["MIN_MIXIN"].as<uint16_t>();
  }
  if (options.count("MANDATORY_MIXIN_BLOCK_VERSION")) {
    MANDATORY_MIXIN_BLOCK_VERSION = options["MANDATORY_MIXIN_BLOCK_VERSION"].as<uint8_t>();
  }
  if (options.count("MIXIN_START_HEIGHT")) {
    MIXIN_START_HEIGHT = options["MIXIN_START_HEIGHT"].as<uint32_t>();
  }
  if (options.count("MANDATORY_TRANSACTION")) {
    MANDATORY_TRANSACTION = options["MANDATORY_TRANSACTION"].as<uint32_t>();
  }
  if (options.count("KILL_HEIGHT")) {
    KILL_HEIGHT = options["KILL_HEIGHT"].as<uint32_t>();
  }
  if (options.count("TAIL_EMISSION_REWARD")) {
    TAIL_EMISSION_REWARD = options["TAIL_EMISSION_REWARD"].as<uint64_t>();
  }
  if (options.count("CRYPTONOTE_COIN_VERSION")) {
    CRYPTONOTE_COIN_VERSION = options["CRYPTONOTE_COIN_VERSION"].as<size_t>();
  }
  if (options.count("GENESIS_BLOCK_REWARD")) {
    GENESIS_BLOCK_REWARD = options["GENESIS_BLOCK_REWARD"].as<uint64_t>();
  }
  if (options.count("ZAWY_DIFFICULTY_BLOCK_INDEX")) {
    ZAWY_DIFFICULTY_BLOCK_INDEX = options["ZAWY_DIFFICULTY_BLOCK_INDEX"].as<uint32_t>();
  }
  if (options.count("ZAWY_DIFFICULTY_LAST_BLOCK")) {
    ZAWY_DIFFICULTY_LAST_BLOCK = options["ZAWY_DIFFICULTY_LAST_BLOCK"].as<uint32_t>();
  }
  if (options.count("ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX")) {
    ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX = options["ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX"].as<uint32_t>();
  }
  if (options.count("ZAWY_LWMA_DIFFICULTY_LAST_BLOCK")) {
    ZAWY_LWMA_DIFFICULTY_LAST_BLOCK = options["ZAWY_LWMA_DIFFICULTY_LAST_BLOCK"].as<uint32_t>();
  }
  if (options.count("ZAWY_LWMA_DIFFICULTY_N")) {
    ZAWY_LWMA_DIFFICULTY_N = options["ZAWY_LWMA_DIFFICULTY_N"].as<size_t>();
  }
  if (options.count("BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX")) {
    BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX = options["BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX"].as<uint32_t>();
  }
  if (options.count("EMISSION_SPEED_FACTOR")) {
    EMISSION_SPEED_FACTOR = options["EMISSION_SPEED_FACTOR"].as<unsigned>();
  }
  if (options.count("CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE")) {
    CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE = options["CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE"].as<size_t>();
  }
  if (options.count("CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1")) {
    CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1 = options["CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1"].as<size_t>();
  }
  if (options.count("CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2")) {
    CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2 = options["CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2"].as<size_t>();
  }
  if (options.count("CRYPTONOTE_DISPLAY_DECIMAL_POINT")) {
    CRYPTONOTE_DISPLAY_DECIMAL_POINT = options["CRYPTONOTE_DISPLAY_DECIMAL_POINT"].as<size_t>();
  }
  if (options.count("MINIMUM_FEE")) {
    MINIMUM_FEE = options["MINIMUM_FEE"].as<uint64_t>();
  }
  if (options.count("DEFAULT_DUST_THRESHOLD")) {
    DEFAULT_DUST_THRESHOLD = options["DEFAULT_DUST_THRESHOLD"].as<uint64_t>();
  }
  if (options.count("DIFFICULTY_TARGET")) {
    DIFFICULTY_TARGET = options["DIFFICULTY_TARGET"].as<uint64_t>();
  }
  if (options.count("CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW")) {
    CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW = options["CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW"].as<uint32_t>();
  }
  if (options.count("MAX_BLOCK_SIZE_INITIAL")) {
    MAX_BLOCK_SIZE_INITIAL = options["MAX_BLOCK_SIZE_INITIAL"].as<size_t>();
  }
  if (options.count("EXPECTED_NUMBER_OF_BLOCKS_PER_DAY")) {
    EXPECTED_NUMBER_OF_BLOCKS_PER_DAY = options["EXPECTED_NUMBER_OF_BLOCKS_PER_DAY"].as<uint64_t>();
  }
  if (options.count("UPGRADE_HEIGHT_V2")) {
    UPGRADE_HEIGHT_V2 = options["UPGRADE_HEIGHT_V2"].as<uint32_t>();
  }
  if (options.count("UPGRADE_HEIGHT_V3")) {
    UPGRADE_HEIGHT_V3 = options["UPGRADE_HEIGHT_V3"].as<uint32_t>();
  }
  if (options.count("KEY_IMAGE_CHECKING_BLOCK_INDEX")) {
    KEY_IMAGE_CHECKING_BLOCK_INDEX = options["KEY_IMAGE_CHECKING_BLOCK_INDEX"].as<uint32_t>();
  }
  if (options.count("DIFFICULTY_WINDOW_V1")) {
    DIFFICULTY_WINDOW_V1 = options["DIFFICULTY_WINDOW_V1"].as<size_t>();
  }
  if (options.count("DIFFICULTY_WINDOW_V2")) {
    DIFFICULTY_WINDOW_V2 = options["DIFFICULTY_WINDOW_V2"].as<size_t>();
  }
  if (options.count("DIFFICULTY_CUT_V1")) {
    DIFFICULTY_CUT_V1 = options["DIFFICULTY_CUT_V1"].as<size_t>();
  }
  if (options.count("DIFFICULTY_CUT_V2")) {
    DIFFICULTY_CUT_V2 = options["DIFFICULTY_CUT_V2"].as<size_t>();
  }
  if (options.count("DIFFICULTY_LAG_V1")) {
    DIFFICULTY_LAG_V1 = options["DIFFICULTY_LAG_V1"].as<size_t>();
  }
  if (options.count("DIFFICULTY_LAG_V2")) {
    DIFFICULTY_LAG_V2 = options["DIFFICULTY_LAG_V2"].as<size_t>();
  }
  if (options.count("DIFFICULTY_WINDOW")) {
    DIFFICULTY_WINDOW = options["DIFFICULTY_WINDOW"].as<size_t>();
  }
  if (options.count("MAX_TRANSACTION_SIZE_LIMIT")) {
    MAX_TRANSACTION_SIZE_LIMIT = options["MAX_TRANSACTION_SIZE_LIMIT"].as<uint64_t>();
  }
  if (options.count("DIFFICULTY_CUT")) {
    DIFFICULTY_CUT = options["DIFFICULTY_CUT"].as<size_t>();
  }
  if (options.count("DIFFICULTY_LAG")) {
    DIFFICULTY_LAG = options["DIFFICULTY_LAG"].as<size_t>();
  }
}

} //namespace PaymentService
