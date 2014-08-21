// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#define CRYPTONOTE_MAX_BLOCK_NUMBER                     500000000
#define CRYPTONOTE_MAX_BLOCK_SIZE                       500000000  // block header blob limit, never used!
#define CRYPTONOTE_MAX_TX_SIZE                          1000000000
#define CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER          0
#define CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX         0xDB // addresses start with "dd"
#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW            6
#define CURRENT_TRANSACTION_VERSION                     1
#define CURRENT_BLOCK_MAJOR_VERSION                     1
#define CURRENT_BLOCK_MINOR_VERSION                     0
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT              60*60*2
#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW               30

// MONEY_SUPPLY - total number coins to be generated
#define MONEY_SUPPLY                                    UINT64_C(858986905600000000)
// COIN - number of smallest units in one coin
#define COIN                                            ((uint64_t)100000000) // pow(10, 8)
#define DEFAULT_FEE                                     ((uint64_t)10000000) // pow(10, 7)
#define MINIMUM_FEE                                     ((uint64_t)10000000) // pow(10, 7)

#define START_BLOCK_REWARD                              (UINT64_C(320000) * COIN)
#define MIN_BLOCK_REWARD                                (UINT64_C(150) * COIN)
#define REWARD_HALVING_INTERVAL                         (UINT64_C(11000))

#define CRYPTONOTE_REWARD_BLOCKS_WINDOW                 100
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE       32000 /*!!!*/ //size of block (bytes) after which reward for block calculated using block size
#define CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE          600
#define CRYPTONOTE_DISPLAY_DECIMAL_POINT                8

#define DIFFICULTY_TARGET                               240 // seconds
#define DIFFICULTY_WINDOW                               240 // blocks
#define DIFFICULTY_LAG                                  15  // !!!
#define DIFFICULTY_CUT                                  30  // timestamps to cut after sorting
#define DIFFICULTY_BLOCKS_COUNT                         DIFFICULTY_WINDOW + DIFFICULTY_LAG


#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS      DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS       1

#define CRYPTONOTE_MEMPOOL_TX_LIVETIME                  (60*60*24) //seconds, one day
#define CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME   (60*60*24*7) //seconds, one week

#define DIFFICULTY_BLOCKS_ESTIMATE_TIMESPAN             DIFFICULTY_TARGET //just alias


#define BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT          10000  //by default, blocks ids count in synchronizing
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT              200    //by default, blocks count in blocks downloading
#define CRYPTONOTE_PROTOCOL_HOP_RELAX_COUNT             3      //value of hop, after which we use only announce of new block


#define P2P_DEFAULT_PORT                                42080
#define RPC_DEFAULT_PORT                                42081
#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT           1000

#define P2P_LOCAL_WHITE_PEERLIST_LIMIT                  1000
#define P2P_LOCAL_GRAY_PEERLIST_LIMIT                   5000

#define P2P_DEFAULT_CONNECTIONS_COUNT                   8
#define P2P_DEFAULT_HANDSHAKE_INTERVAL                  60           //secondes
#define P2P_DEFAULT_PACKET_MAX_SIZE                     50000000     //50000000 bytes maximum packet size
#define P2P_DEFAULT_PEERS_IN_HANDSHAKE                  250
#define P2P_DEFAULT_CONNECTION_TIMEOUT                  5000       //5 seconds
#define P2P_DEFAULT_PING_CONNECTION_TIMEOUT             3000       //3 seconds
#define P2P_DEFAULT_INVOKE_TIMEOUT                      60*2*1000  //2 minutes
#define P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT            5000       //5 seconds
#define P2P_STAT_TRUSTED_PUB_KEY                        "85ae8734f90bc1ee295ceb0ec05a49852d4dbbc9d1c27a619b5f4bdf26a0196e"
#define P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT       70

#define ALLOW_DEBUG_COMMANDS

#define CRYPTONOTE_NAME                         "ducknote"
#define CRYPTONOTE_POOLDATA_FILENAME            "poolstate.bin"
#define CRYPTONOTE_BLOCKCHAINDATA_FILENAME      "blockchain.bin"
#define CRYPTONOTE_BLOCKCHAINDATA_TEMP_FILENAME "blockchain.bin.tmp"
#define P2P_NET_DATA_FILENAME                   "p2pstate.bin"
#define MINER_CONFIG_FILE_NAME                  "miner_conf.json"

#define THREAD_STACK_SIZE                       5 * 1024 * 1024
