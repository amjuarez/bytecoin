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

#pragma once

#include <cstdint>
#include <boost/uuid/uuid.hpp>

namespace cryptonote {
namespace parameters {

const uint64_t CRYPTONOTE_MAX_BLOCK_NUMBER                   = 500000000;
const size_t   CRYPTONOTE_MAX_BLOCK_BLOB_SIZE                = 500000000;
const size_t   CRYPTONOTE_MAX_TX_SIZE                        = 1000000000;
const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX       = 0xDB; // addresses start with "0xDB"
const size_t   CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW          = 6;
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT            = 60 * 60 * 2;

const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW             = 30;

// MONEY_SUPPLY - total number coins to be generated
const uint64_t MONEY_SUPPLY                                  = UINT64_C(858986905600000000);

const char     GENESIS_COINBASE_HEX[]                        = "010601ff0001808088a5a9a307029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd088071210138dc57b313e2560fa75f5d7c9a6398800855220aefb3603bc70826adc83e0cc1";
const uint32_t GENESIS_NONCE                                 = 420;

const size_t   CRYPTONOTE_REWARD_BLOCKS_WINDOW               = 100;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE     = 32000; //size of block (bytes) after which reward for block calculated using block size
const size_t   CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE        = 600;
const size_t   CRYPTONOTE_DISPLAY_DECIMAL_POINT              = 8;
// COIN - number of smallest units in one coin
const uint64_t COIN                                          = UINT64_C(100000000);  // pow(10, 8)
const uint64_t MINIMUM_FEE                                   = UINT64_C(100000);     // pow(10, 5)
const uint64_t DEFAULT_DUST_THRESHOLD                        = UINT64_C(100000);     // pow(10, 5)

const uint64_t DIFFICULTY_TARGET                             = 240; // seconds
const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY             = 24 * 60 * 60 / DIFFICULTY_TARGET;
const size_t   DIFFICULTY_WINDOW                             = 240; // blocks
const size_t   DIFFICULTY_CUT                                = 30;  // timestamps to cut after sorting
const size_t   DIFFICULTY_LAG                                = 15;  // !!!
static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Bad DIFFICULTY_WINDOW or DIFFICULTY_CUT");

const size_t   MAX_BLOCK_SIZE_INITIAL                        = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE * 10;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR         = 100 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR       = 365 * 24 * 60 * 60 / DIFFICULTY_TARGET;

const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS     = 1;
const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS    = DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME                = (60 * 60 * 14); //seconds, 14 hours
const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME = (60 * 60 * 24); //seconds, one day

const char     CRYPTONOTE_BLOCKCHAINDATA_FILENAME[]          = "blockchain.bin";    // Obsolete blockchain format
const char     CRYPTONOTE_BLOCKS_FILENAME[]                  = "blocks.dat";
const char     CRYPTONOTE_BLOCKINDEXES_FILENAME[]            = "blockindexes.dat";
const char     CRYPTONOTE_BLOCKSCACHE_FILENAME[]             = "blockscache.dat";
const char     CRYPTONOTE_POOLDATA_FILENAME[]                = "poolstate.bin";
const char     P2P_NET_DATA_FILENAME[]                       = "p2pstate.bin";
const char     MINER_CONFIG_FILE_NAME[]                      = "miner_conf.json";
} // parameters

const uint64_t START_BLOCK_REWARD                            = (UINT64_C(320000) * parameters::COIN);
const uint64_t MIN_BLOCK_REWARD                              = (UINT64_C(150) * parameters::COIN);
const uint64_t REWARD_HALVING_INTERVAL                       = (UINT64_C(11000));

const char     CRYPTONOTE_NAME[]                             = "ducknote";

const uint8_t  CURRENT_TRANSACTION_VERSION                   =  1;
const uint8_t  BLOCK_MAJOR_VERSION_1                         =  1;
const uint8_t  BLOCK_MINOR_VERSION_0                         =  0;
const uint8_t  BLOCK_MINOR_VERSION_1                         =  1;
const uint8_t  CURRENT_BLOCK_MAJOR_VERSION                   =  1;
const uint8_t  CURRENT_BLOCK_MINOR_VERSION                   =  0;

const size_t   BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT        =  10000;  //by default, blocks ids count in synchronizing
const size_t   BLOCKS_SYNCHRONIZING_DEFAULT_COUNT            =  200;    //by default, blocks count in blocks downloading
const size_t   COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT         =  1000;

const int      P2P_DEFAULT_PORT                              =  42080;
const int      RPC_DEFAULT_PORT                              =  42081;

const size_t   P2P_LOCAL_WHITE_PEERLIST_LIMIT                =  1000;
const size_t   P2P_LOCAL_GRAY_PEERLIST_LIMIT                 =  5000;

const uint32_t P2P_DEFAULT_CONNECTIONS_COUNT                 = 8;
const uint32_t P2P_DEFAULT_HANDSHAKE_INTERVAL                = 60;            // seconds
const uint32_t P2P_DEFAULT_PACKET_MAX_SIZE                   = 50000000;      // 50000000 bytes maximum packet size
const uint32_t P2P_DEFAULT_PEERS_IN_HANDSHAKE                = 250;
const uint32_t P2P_DEFAULT_CONNECTION_TIMEOUT                = 5000;          // 5 seconds
const uint32_t P2P_DEFAULT_PING_CONNECTION_TIMEOUT           = 2000;          // 2 seconds
const uint64_t P2P_DEFAULT_INVOKE_TIMEOUT                    = 60 * 2 * 1000; // 2 minutes
const size_t   P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT          = 5000;          // 5 seconds
const char     P2P_STAT_TRUSTED_PUB_KEY[]                    = "85ae8734f90bc1ee295ceb0ec05a49852d4dbbc9d1c27a619b5f4bdf26a0196e";
const size_t   P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT     = 70;

const boost::uuids::uuid NETWORK_ID                          = { { 0x4E, 0x6F, 0x73, 0x63, 0x65, 0x20, 0x74, 0x65, 0x20, 0x69, 0x70, 0x73, 0x75, 0x6D, 0x20, 0x20} };

const unsigned THREAD_STACK_SIZE                             = 5 * 1024 * 1024;

const char* const SEED_NODES[] = {
  "seed.ducknote.org:42080"
};

struct CheckpointData {
  uint64_t height;
  const char* blockId;
};

const CheckpointData CHECKPOINTS[] = {
    { 1100, "990a83b3e77ba5def86311da34793e09fa3b0a2875571bd59449173fddf4e129" },
    { 4200, "76af92fc41eadf9c99df91efc08011d0fce6f3f55b131da2449c187f432f91f7" },
    { 11000, "970c15459e4d484166c36e303fcf35886e14633b40b1fe4e3f250eb03eaca1f8" },
    { 22000, "ae9ab36c4ff2cf215d49ffa4358d108dd777b8897c2d873a064dc103fea2b5ab" },
    { 33000, "3fac95a900e65391d693e2cb331a26c757595baac133b9fa24936dd50fc7465f" },
    { 44000, "071a97648427ad25ec206ae7101534c9b011376f05dee04780b5edb22f9a919e" },
    { 47000, "a2fda9ea94260ed7177aec5b74802606bc7800d4a1713f761ac71a0883b4b480" },
    { 55000, "57f48bc9b2dddace94bddc8858cf1cdf5e68cc0db763d7ebcab71b388755e0ce" },
    { 66000, "90a2bc9e75503d386d41c48e698d474c337291f8c4417d63e90a8b5727f06320" }
};

} // cryptonote

#define ALLOW_DEBUG_COMMANDS
