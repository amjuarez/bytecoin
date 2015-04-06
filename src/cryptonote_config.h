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

namespace cryptonote {
namespace parameters {

const uint64_t CRYPTONOTE_MAX_BLOCK_NUMBER                   = 500000000;
const size_t   CRYPTONOTE_MAX_BLOCK_BLOB_SIZE                = 500000000;
const size_t   CRYPTONOTE_MAX_TX_SIZE                        = 1000000000;
const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX       = 6; // addresses start with "2"
const size_t   CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW          = 10;
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT            = 60 * 60 * 2;

const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW             = 60;

// MONEY_SUPPLY - total number coins to be generated
const uint64_t MONEY_SUPPLY                                  = static_cast<uint64_t>(-1);
const unsigned EMISSION_SPEED_FACTOR                         = 18;
static_assert(EMISSION_SPEED_FACTOR <= 8 * sizeof(uint64_t), "Bad EMISSION_SPEED_FACTOR");

const size_t   CRYPTONOTE_REWARD_BLOCKS_WINDOW               = 100;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE     = 20000; //size of block (bytes) after which reward for block calculated using block size
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1  = 10000;
const size_t   CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE        = 600;
const size_t   CRYPTONOTE_DISPLAY_DECIMAL_POINT              = 8;
// COIN - number of smallest units in one coin
const uint64_t COIN                                          = UINT64_C(100000000);  // pow(10, 8)
const uint64_t MINIMUM_FEE                                   = UINT64_C(1000000);    // pow(10, 6)
const uint64_t DEFAULT_DUST_THRESHOLD                        = UINT64_C(1000000);    // pow(10, 6)

const uint64_t DIFFICULTY_TARGET                             = 120; // seconds
const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY             = 24 * 60 * 60 / DIFFICULTY_TARGET;
const size_t   DIFFICULTY_WINDOW                             = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; // blocks
const size_t   DIFFICULTY_CUT                                = 60;  // timestamps to cut after sorting
const size_t   DIFFICULTY_LAG                                = 15;  // !!!
static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Bad DIFFICULTY_WINDOW or DIFFICULTY_CUT");

const size_t   MAX_BLOCK_SIZE_INITIAL                        =  20 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR         = 100 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR       = 365 * 24 * 60 * 60 / DIFFICULTY_TARGET;

const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS     = 1;
const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS    = DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME                = 60 * 60 * 24;     //seconds, one day
const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME = 60 * 60 * 24 * 7; //seconds, one week

const uint64_t UPGRADE_HEIGHT                                = 546602;
const unsigned UPGRADE_VOTING_THRESHOLD                      = 90;               // percent
const size_t   UPGRADE_VOTING_WINDOW                         = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;  // blocks
const size_t   UPGRADE_WINDOW                                = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;  // blocks
static_assert(0 < UPGRADE_VOTING_THRESHOLD && UPGRADE_VOTING_THRESHOLD <= 100, "Bad UPGRADE_VOTING_THRESHOLD");
static_assert(UPGRADE_VOTING_WINDOW > 1, "Bad UPGRADE_VOTING_WINDOW");

const char     CRYPTONOTE_BLOCKCHAINDATA_FILENAME[]          = "blockchain.bin";    // Obsolete blockchain format
const char     CRYPTONOTE_BLOCKS_FILENAME[]                  = "blocks.dat";
const char     CRYPTONOTE_BLOCKINDEXES_FILENAME[]            = "blockindexes.dat";
const char     CRYPTONOTE_BLOCKSCACHE_FILENAME[]             = "blockscache.dat";
const char     CRYPTONOTE_POOLDATA_FILENAME[]                = "poolstate.bin";
const char     P2P_NET_DATA_FILENAME[]                       = "p2pstate.bin";
const char     MINER_CONFIG_FILE_NAME[]                      = "miner_conf.json";
} // parameters

const char     CRYPTONOTE_NAME[]                             = "bytecoin";

const uint8_t  CURRENT_TRANSACTION_VERSION                   =  1;
const uint8_t  BLOCK_MAJOR_VERSION_1                         =  1;
const uint8_t  BLOCK_MAJOR_VERSION_2                         =  2;
const uint8_t  BLOCK_MINOR_VERSION_0                         =  0;
const uint8_t  BLOCK_MINOR_VERSION_1                         =  1;

const size_t   BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT        =  10000;  //by default, blocks ids count in synchronizing
const size_t   BLOCKS_SYNCHRONIZING_DEFAULT_COUNT            =  200;    //by default, blocks count in blocks downloading
const size_t   COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT         =  1000;

const int      P2P_DEFAULT_PORT                              =  8080;
const int      RPC_DEFAULT_PORT                              =  8081;

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
const char     P2P_STAT_TRUSTED_PUB_KEY[]                    = "93467628927eaa0b13a4e52e61864a75aa475e67f6b5748eb3fc1d2fe468aed4";
const size_t   P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT     = 70;

const unsigned THREAD_STACK_SIZE                             = 5 * 1024 * 1024;

const char* const SEED_NODES[] = {
  "seed.bytecoin.org:8080",
  "85.25.201.95:8080",
  "85.25.196.145:8080",
  "85.25.196.146:8080",
  "85.25.196.144:8080",
  "5.199.168.138:8080",
  "62.75.236.152:8080",
  "85.25.194.245:8080",
  "95.211.224.160:8080",
  "144.76.200.44:8080"
};

struct CheckpointData {
  uint64_t height;
  const char* blockId;
};

const CheckpointData CHECKPOINTS[] = {
  {79000,  "cae33204e624faeb64938d80073bb7bbacc27017dc63f36c5c0f313cad455a02"},
  {140000, "993059fb6ab92db7d80d406c67a52d9c02d873ca34b6290a12b744c970208772"},
  {200000, "a5f74c7542077df6859f48b5b1f9c3741f29df38f91a47e14c94b5696e6c3073"},
  {230580, "32bd7cb6c68a599cf2861941f29002a5e203522b9af54f08dfced316f6459103"},
  {260000, "f68e70b360ca194f48084da7a7fd8e0251bbb4b5587f787ca65a6f5baf3f5947"},
  {300000, "8e80861713f68354760dc10ea6ea79f5f3ff28f39b3f0835a8637463b09d70ff"},
  {390285, "e00bdc9bf407aeace2f3109de11889ed25894bf194231d075eddaec838097eb7"},
  {417000, "2dc96f8fc4d4a4d76b3ed06722829a7ab09d310584b8ecedc9b578b2c458a69f"},
  {427193, "00feabb08f2d5759ed04fd6b799a7513187478696bba2db2af10d4347134e311"},
  {453537, "d17de6916c5aa6ffcae575309c80b0f8fdcd0a84b5fa8e41a841897d4b5a4e97"},
  {462250, "13468d210a5ec884cf839f0259f247ccf3efef0414ac45172033d32c739beb3e"},
  {468000, "251bcbd398b1f593193a7210934a3d87f692b2cb0c45206150f59683dd7e9ba1"},
  {480200, "363544ac9920c778b815c2fdbcbca70a0d79b21f662913a42da9b49e859f0e5b"},
  {484500, "5cdf2101a0a62a0ab2a1ca0c15a6212b21f6dbdc42a0b7c0bcf65ca40b7a14fb"},
  {506000, "3d54c1132f503d98d3f0d78bb46a4503c1a19447cb348361a2232e241cb45a3c"},
  {544000, "f69dc61b6a63217f32fa64d5d0f9bd920873f57dfd79ebe1d7d6fb1345b56fe0"},
  {553300, "f7a5076b887ce5f4bb95b2729c0edb6f077a463f04f1bffe7f5cb0b16bb8aa5f"},
  {580000, "93aea06936fa4dc0a84c9109c9d5f0e1b0815f96898171e42fd2973d262ed9ac"},
  {602000, "a05fd2fccbb5f567ece940ebb62a82fdb1517ff5696551ae704e5f0ef8edb979"},
  {623000, "7c92dd374efd0221065c7d98fce0568a1a1c130b5da28bb3f338cdc367b93d0b"},
  {645000, "1eeba944c0dd6b9a1228a425a74076fbdbeaf9b657ba7ef02547d99f971de70d"},
  {667000, "a020c8fcaa567845d04b520bb7ebe721e097a9bed2bdb8971081f933b5b42995"},
  {689000, "212ec2698c5ebd15d6242d59f36c2d186d11bb47c58054f476dd8e6b1c7f0008"},
  {713000, "a03f836c4a19f907cd6cac095eb6f56f5279ca2d1303fb7f826750dcb9025495"}
};
} // cryptonote

#define ALLOW_DEBUG_COMMANDS
