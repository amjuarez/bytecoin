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

#pragma once

#include <cstdint>
#include <boost/program_options.hpp>
#include <stddef.h>
#include <initializer_list>
#include "CryptoNoteConfig.h"

namespace PaymentService {

class CoinBaseConfiguration {
public:
  CoinBaseConfiguration();

  static void initOptions(boost::program_options::options_description& desc);
  void init(const boost::program_options::variables_map& options);

  std::string CRYPTONOTE_NAME;
  std::string GENESIS_COINBASE_TX_HEX;
  uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
  uint64_t MONEY_SUPPLY;
  uint32_t BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX;
  uint32_t ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX;
  uint32_t ZAWY_LWMA_DIFFICULTY_LAST_BLOCK;
  size_t ZAWY_LWMA_DIFFICULTY_N;
  uint32_t ZAWY_DIFFICULTY_BLOCK_INDEX;
  uint32_t ZAWY_DIFFICULTY_LAST_BLOCK;
  uint64_t GENESIS_BLOCK_REWARD;
  size_t CRYPTONOTE_COIN_VERSION;
  uint64_t TAIL_EMISSION_REWARD;
  uint32_t KILL_HEIGHT;
  uint32_t MANDATORY_TRANSACTION;
  uint32_t MIXIN_START_HEIGHT;
  uint16_t MIN_MIXIN;
  uint8_t MANDATORY_MIXIN_BLOCK_VERSION;
  unsigned int EMISSION_SPEED_FACTOR;
  size_t CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
  size_t CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1;
  size_t CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2;
  uint64_t CRYPTONOTE_DISPLAY_DECIMAL_POINT;
  uint64_t MINIMUM_FEE;
  uint64_t DEFAULT_DUST_THRESHOLD;
  uint64_t DIFFICULTY_TARGET;
  uint32_t CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;
  size_t MAX_BLOCK_SIZE_INITIAL;
  uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;
  uint32_t UPGRADE_HEIGHT_V2;
  uint32_t UPGRADE_HEIGHT_V3;
  uint32_t KEY_IMAGE_CHECKING_BLOCK_INDEX;
  size_t DIFFICULTY_WINDOW_V1;
  size_t DIFFICULTY_WINDOW_V2;
  size_t DIFFICULTY_LAG_V1;
  size_t DIFFICULTY_LAG_V2;
  size_t DIFFICULTY_CUT_V1;
  size_t DIFFICULTY_CUT_V2;
  size_t DIFFICULTY_WINDOW;
uint64_t MAX_TRANSACTION_SIZE_LIMIT;
  size_t DIFFICULTY_CUT;
  size_t DIFFICULTY_LAG;
};

} //namespace PaymentService
