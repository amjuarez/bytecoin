// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
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

#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include "DaemonCommandsHandler.h"

#include "Common/ScopeExit.h"
#include "Common/SignalHandler.h"
#include "Common/StdOutputStream.h"
#include "Common/StdInputStream.h"
#include "Common/PathTools.h"
#include "Common/Util.h"
#include "crypto/hash.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/DatabaseBlockchainCache.h"
#include "CryptoNoteCore/DatabaseBlockchainCacheFactory.h"
#include "CryptoNoteCore/MainChainStorage.h"
#include "CryptoNoteCore/MinerConfig.h"
#include "CryptoNoteCore/RocksDBWrapper.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "P2p/NetNode.h"
#include "P2p/NetNodeConfig.h"
#include "Rpc/RpcServer.h"
#include "Rpc/RpcServerConfig.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"
#include "version.h"

#include <Logging/LoggerManager.h>

#if defined(WIN32)
#include <crtdbg.h>
#endif

using Common::JsonValue;
using namespace CryptoNote;
using namespace Logging;

namespace po = boost::program_options;

namespace
{
  const command_line::arg_descriptor<std::string> arg_config_file = {"config-file", "Specify configuration file", ""};
  const command_line::arg_descriptor<bool>        arg_os_version  = {"os-version", ""};
  const command_line::arg_descriptor<std::string> arg_log_file    = {"log-file", "", ""};
  const command_line::arg_descriptor<int>         arg_log_level   = {"log-level", "", 2}; // info level
  const command_line::arg_descriptor<bool>        arg_console     = {"no-console", "Disable daemon console commands"};
  const command_line::arg_descriptor<std::string> arg_set_fee_address = { "fee-address", "Sets fee address for light wallets to the daemon's RPC responses.", "" };
  const command_line::arg_descriptor<bool>        arg_print_genesis_tx = { "print-genesis-tx", "Prints genesis' block tx hex to insert it to config and exits" };
  const command_line::arg_descriptor<std::vector<std::string>> arg_genesis_block_reward_address = { "genesis-block-reward-address", "" };
  const command_line::arg_descriptor<bool> arg_blockexplorer_on = {"enable-blockexplorer", "Enable blockchain explorer RPC", false};
  const command_line::arg_descriptor<std::vector<std::string>>        arg_enable_cors = { "enable-cors", "Adds header 'Access-Control-Allow-Origin' to the daemon's RPC responses. Uses the value as domain. Use * for all" };
  const command_line::arg_descriptor<std::string> arg_GENESIS_COINBASE_TX_HEX  = {"GENESIS_COINBASE_TX_HEX", "Genesis transaction hex", CryptoNote::parameters::GENESIS_COINBASE_TX_HEX};  
  const command_line::arg_descriptor<uint64_t>    arg_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX  = {"CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX", "uint64_t", CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX};
  const command_line::arg_descriptor<uint64_t>    arg_MONEY_SUPPLY  = {"MONEY_SUPPLY", "uint64_t", CryptoNote::parameters::MONEY_SUPPLY};
  const command_line::arg_descriptor<unsigned>    arg_EMISSION_SPEED_FACTOR  = {"EMISSION_SPEED_FACTOR", "unsigned", CryptoNote::parameters::EMISSION_SPEED_FACTOR};
  const command_line::arg_descriptor<size_t>      arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE  = {"CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE", "size_t", CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE};
  const command_line::arg_descriptor<size_t>      arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1  = {"CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1", "size_t", CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1};
  const command_line::arg_descriptor<size_t>      arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2  = {"CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2", "size_t", CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2};
  const command_line::arg_descriptor<uint64_t>    arg_CRYPTONOTE_DISPLAY_DECIMAL_POINT  = {"CRYPTONOTE_DISPLAY_DECIMAL_POINT", "uint64_t", CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT};
  const command_line::arg_descriptor<uint64_t>    arg_MINIMUM_FEE  = {"MINIMUM_FEE", "uint64_t", CryptoNote::parameters::MINIMUM_FEE};
  const command_line::arg_descriptor<uint64_t>    arg_DEFAULT_DUST_THRESHOLD  = {"DEFAULT_DUST_THRESHOLD", "uint64_t", CryptoNote::parameters::DEFAULT_DUST_THRESHOLD};
  const command_line::arg_descriptor<uint64_t>    arg_DIFFICULTY_TARGET  = {"DIFFICULTY_TARGET", "uint64_t", CryptoNote::parameters::DIFFICULTY_TARGET};
  const command_line::arg_descriptor<uint32_t>    arg_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW  = {"CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW", "uint32_t", CryptoNote::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW};
  const command_line::arg_descriptor<size_t>      arg_MAX_BLOCK_SIZE_INITIAL  = {"MAX_BLOCK_SIZE_INITIAL", "size_t", CryptoNote::parameters::MAX_BLOCK_SIZE_INITIAL};
  const command_line::arg_descriptor<uint64_t>    arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY  = {"EXPECTED_NUMBER_OF_BLOCKS_PER_DAY", "uint64_t"};
  const command_line::arg_descriptor<uint32_t>    arg_UPGRADE_HEIGHT_V2  = {"UPGRADE_HEIGHT_V2", "uint32_t", 0};
  const command_line::arg_descriptor<uint32_t>    arg_UPGRADE_HEIGHT_V3  = {"UPGRADE_HEIGHT_V3", "uint32_t", 0};
  const command_line::arg_descriptor<uint32_t>    arg_KEY_IMAGE_CHECKING_BLOCK_INDEX  = {"KEY_IMAGE_CHECKING_BLOCK_INDEX", "uint32_t", 0};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_WINDOW_V1  = {"DIFFICULTY_WINDOW_V1", "size_t", 0};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_WINDOW_V2  = {"DIFFICULTY_WINDOW_V2", "size_t", 0};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_CUT_V1  = {"DIFFICULTY_CUT_V1", "size_t", CryptoNote::parameters::DIFFICULTY_CUT};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_CUT_V2  = {"DIFFICULTY_CUT_V2", "size_t", CryptoNote::parameters::DIFFICULTY_CUT};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_LAG_V1  = {"DIFFICULTY_LAG_V1", "size_t", CryptoNote::parameters::DIFFICULTY_LAG};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_LAG_V2  = {"DIFFICULTY_LAG_V2", "size_t", CryptoNote::parameters::DIFFICULTY_LAG};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_WINDOW  = {"DIFFICULTY_WINDOW", "size_t", 0};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_CUT  = {"DIFFICULTY_CUT", "size_t", CryptoNote::parameters::DIFFICULTY_CUT};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_LAG  = {"DIFFICULTY_LAG", "size_t", CryptoNote::parameters::DIFFICULTY_LAG};
  const command_line::arg_descriptor<std::string> arg_CRYPTONOTE_NAME  = {"CRYPTONOTE_NAME", "Cryptonote name. Used for storage directory", ""};
  const command_line::arg_descriptor< std::vector<std::string> > arg_CHECKPOINT  = {"CHECKPOINT", "Checkpoints. Format: HEIGHT:HASH"};
  const command_line::arg_descriptor<uint32_t>    arg_BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX  = {"BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX", "uint32_t", 0};
  const command_line::arg_descriptor<uint32_t>    arg_ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX = {"ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX", "uint32_t", 0};
  const command_line::arg_descriptor<uint32_t>    arg_ZAWY_LWMA_DIFFICULTY_LAST_BLOCK  = {"ZAWY_LWMA_DIFFICULTY_LAST_BLOCK", "uint32_t", 0};
  const command_line::arg_descriptor<size_t>    arg_ZAWY_LWMA_DIFFICULTY_N  = {"ZAWY_LWMA_DIFFICULTY_N", "size_t", 0};
  const command_line::arg_descriptor<uint32_t>    arg_ZAWY_DIFFICULTY_BLOCK_INDEX  = {"ZAWY_DIFFICULTY_BLOCK_INDEX", "uint32_t", 0};
  const command_line::arg_descriptor<uint32_t>    arg_ZAWY_DIFFICULTY_LAST_BLOCK  = {"ZAWY_DIFFICULTY_LAST_BLOCK", "uint32_t", 0};
  const command_line::arg_descriptor<uint64_t>    arg_GENESIS_BLOCK_REWARD  = {"GENESIS_BLOCK_REWARD", "uint64_t", 0};
  const command_line::arg_descriptor<size_t>    arg_CRYPTONOTE_COIN_VERSION  = {"CRYPTONOTE_COIN_VERSION", "size_t", 0};
  const command_line::arg_descriptor<uint64_t>    arg_TAIL_EMISSION_REWARD  = {"TAIL_EMISSION_REWARD", "uint64_t", 0};
  const command_line::arg_descriptor<uint32_t>    arg_KILL_HEIGHT  = {"KILL_HEIGHT", "uint32_t", 0};
  const command_line::arg_descriptor<uint32_t>    arg_MANDATORY_TRANSACTION  = {"MANDATORY_TRANSACTION", "uint32_t", CryptoNote::parameters::MANDATORY_TRANSACTION};
  const command_line::arg_descriptor<uint32_t>    arg_MIXIN_START_HEIGHT  = {"MIXIN_START_HEIGHT", "uint32_t", 0};
  const command_line::arg_descriptor<uint16_t>    arg_MIN_MIXIN  = {"MIN_MIXIN", "uint16_t", CryptoNote::parameters::MIN_MIXIN};
  const command_line::arg_descriptor<uint8_t>    arg_MANDATORY_MIXIN_BLOCK_VERSION  = {"MANDATORY_MIXIN_BLOCK_VERSION", "uint8_t", CryptoNote::parameters::MANDATORY_MIXIN_BLOCK_VERSION};
  const command_line::arg_descriptor<bool>        arg_testnet_on  = {"testnet", "Used to deploy test nets. Checkpoints and hardcoded seeds are ignored, "
    "network id is changed. Use it with --data-dir flag. The wallet must be launched with --testnet flag.", false};
}

bool command_line_preprocessor(const boost::program_options::variables_map& vm, LoggerRef& logger);
void print_genesis_tx_hex(const po::variables_map& vm, LoggerManager& logManager) {
  std::vector<CryptoNote::AccountPublicAddress> targets;
  auto genesis_block_reward_addresses = command_line::get_arg(vm, arg_genesis_block_reward_address);
  CryptoNote::CurrencyBuilder currencyBuilder(logManager);
    currencyBuilder.cryptonoteName(command_line::get_arg(vm, arg_CRYPTONOTE_NAME));
  currencyBuilder.minMixin(command_line::get_arg(vm, arg_MIN_MIXIN));
//uint8_t recognized as char
  if (command_line::get_arg(vm, arg_MANDATORY_MIXIN_BLOCK_VERSION) == 0) {
    currencyBuilder.mandatoryMixinBlockVersion(command_line::get_arg(vm, arg_MANDATORY_MIXIN_BLOCK_VERSION));
  } else {
    currencyBuilder.mandatoryMixinBlockVersion(command_line::get_arg(vm, arg_MANDATORY_MIXIN_BLOCK_VERSION) - '0');
  }
  currencyBuilder.mandatoryTransaction(command_line::get_arg(vm, arg_MANDATORY_TRANSACTION));
    currencyBuilder.genesisCoinbaseTxHex(command_line::get_arg(vm, arg_GENESIS_COINBASE_TX_HEX));
    currencyBuilder.publicAddressBase58Prefix(command_line::get_arg(vm, arg_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX));
    currencyBuilder.moneySupply(command_line::get_arg(vm, arg_MONEY_SUPPLY));
    currencyBuilder.emissionSpeedFactor(command_line::get_arg(vm, arg_EMISSION_SPEED_FACTOR));
    currencyBuilder.blockGrantedFullRewardZone(command_line::get_arg(vm, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE));
    currencyBuilder.blockGrantedFullRewardZoneV1(command_line::get_arg(vm, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1));
    currencyBuilder.blockGrantedFullRewardZoneV2(command_line::get_arg(vm, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2));
    currencyBuilder.numberOfDecimalPlaces(command_line::get_arg(vm, arg_CRYPTONOTE_DISPLAY_DECIMAL_POINT));
    currencyBuilder.mininumFee(command_line::get_arg(vm, arg_MINIMUM_FEE));
    currencyBuilder.defaultDustThreshold(command_line::get_arg(vm, arg_DEFAULT_DUST_THRESHOLD));
    currencyBuilder.difficultyTarget(command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    currencyBuilder.minedMoneyUnlockWindow(command_line::get_arg(vm, arg_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW));
    currencyBuilder.maxBlockSizeInitial(command_line::get_arg(vm, arg_MAX_BLOCK_SIZE_INITIAL));
    if (command_line::has_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY) && command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY) != 0)
    {
      currencyBuilder.expectedNumberOfBlocksPerDay(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
      currencyBuilder.difficultyWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    currencyBuilder.difficultyWindowV1(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    currencyBuilder.difficultyWindowV2(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
      currencyBuilder.upgradeVotingWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
      currencyBuilder.upgradeWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    } else {
      currencyBuilder.expectedNumberOfBlocksPerDay(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
      currencyBuilder.difficultyWindow(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    currencyBuilder.difficultyWindowV1(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    currencyBuilder.difficultyWindowV2(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    }
    currencyBuilder.maxBlockSizeGrowthSpeedDenominator(365 * 24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    currencyBuilder.lockedTxAllowedDeltaSeconds(command_line::get_arg(vm, arg_DIFFICULTY_TARGET) * CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);  
    if (command_line::has_arg(vm, arg_UPGRADE_HEIGHT_V2) && command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V2) != 0)
    {
      currencyBuilder.upgradeHeightV2(command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V2));
    }
    if (command_line::has_arg(vm, arg_UPGRADE_HEIGHT_V3) && command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V3) != 0)
    {
      currencyBuilder.upgradeHeightV3(command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V3));
    }
    if (command_line::has_arg(vm, arg_KEY_IMAGE_CHECKING_BLOCK_INDEX) && command_line::get_arg(vm, arg_KEY_IMAGE_CHECKING_BLOCK_INDEX) != 0)
    {
      currencyBuilder.keyImageCheckingBlockIndex(command_line::get_arg(vm, arg_KEY_IMAGE_CHECKING_BLOCK_INDEX));
    }
    if (command_line::has_arg(vm, arg_DIFFICULTY_WINDOW) && command_line::get_arg(vm, arg_DIFFICULTY_WINDOW) != 0)
    {
      currencyBuilder.difficultyWindow(command_line::get_arg(vm, arg_DIFFICULTY_WINDOW));
    }
    currencyBuilder.difficultyLag(command_line::get_arg(vm, arg_DIFFICULTY_LAG));
    currencyBuilder.difficultyCut(command_line::get_arg(vm, arg_DIFFICULTY_CUT));
  if (command_line::has_arg(vm, arg_DIFFICULTY_WINDOW_V1) && command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V1) != 0)
  {
    currencyBuilder.difficultyWindowV1(command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V1));
  }
  if (command_line::has_arg(vm, arg_DIFFICULTY_WINDOW_V2) && command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V2) != 0)
  {
    currencyBuilder.difficultyWindowV2(command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V2));
  }
  currencyBuilder.difficultyLagV1(command_line::get_arg(vm, arg_DIFFICULTY_LAG_V1));
  currencyBuilder.difficultyLagV2(command_line::get_arg(vm, arg_DIFFICULTY_LAG_V2));
  currencyBuilder.difficultyCutV1(command_line::get_arg(vm, arg_DIFFICULTY_CUT_V1));
  currencyBuilder.difficultyCutV2(command_line::get_arg(vm, arg_DIFFICULTY_CUT_V2));
bool blockexplorer_mode = command_line::get_arg(vm, arg_blockexplorer_on);
currencyBuilder.isBlockexplorer(blockexplorer_mode);
  CryptoNote::Currency currency = currencyBuilder.currency();
  for (const auto& address_string : genesis_block_reward_addresses) {
     CryptoNote::AccountPublicAddress address;
    if (!currency.parseAccountAddressString(address_string, address)) {
      std::cout << "Failed to parse address: " << address_string << std::endl;
      return;
    }
    targets.emplace_back(std::move(address));
  }
  if (targets.empty()) {
    if (CryptoNote::parameters::GENESIS_BLOCK_REWARD > 0) {
      std::cout << "Error: genesis block reward addresses are not defined" << std::endl;
    } else {
  CryptoNote::Transaction tx = currencyBuilder.generateGenesisTransaction();
  std::string tx_hex = Common::toHex(CryptoNote::toBinaryArray(tx));
  std::cout << "Add this line into your coin configuration file as is: " << std::endl;
  std::cout << "GENESIS_COINBASE_TX_HEX=" << tx_hex << std::endl;
    }
  } else {
  CryptoNote::CurrencyBuilder  currencyBuilder(logManager);
  currencyBuilder.genesisCoinbaseTxHex(command_line::get_arg(vm, arg_GENESIS_COINBASE_TX_HEX));
  currencyBuilder.publicAddressBase58Prefix(command_line::get_arg(vm, arg_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX));
  currencyBuilder.moneySupply(command_line::get_arg(vm, arg_MONEY_SUPPLY));
  currencyBuilder.emissionSpeedFactor(command_line::get_arg(vm, arg_EMISSION_SPEED_FACTOR));
  currencyBuilder.blockGrantedFullRewardZone(command_line::get_arg(vm, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE));
  currencyBuilder.blockGrantedFullRewardZoneV1(command_line::get_arg(vm, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1));
  currencyBuilder.blockGrantedFullRewardZoneV2(command_line::get_arg(vm, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2));
  currencyBuilder.numberOfDecimalPlaces(command_line::get_arg(vm, arg_CRYPTONOTE_DISPLAY_DECIMAL_POINT));
  currencyBuilder.mininumFee(command_line::get_arg(vm, arg_MINIMUM_FEE));
  currencyBuilder.defaultDustThreshold(command_line::get_arg(vm, arg_DEFAULT_DUST_THRESHOLD));
  currencyBuilder.difficultyTarget(command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
  currencyBuilder.minedMoneyUnlockWindow(command_line::get_arg(vm, arg_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW));
  currencyBuilder.maxBlockSizeInitial(command_line::get_arg(vm, arg_MAX_BLOCK_SIZE_INITIAL));
  if (command_line::has_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY) && command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY) != 0)
  {
    currencyBuilder.difficultyWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    currencyBuilder.difficultyWindowV1(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    currencyBuilder.difficultyWindowV2(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    currencyBuilder.upgradeVotingWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    currencyBuilder.upgradeWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
  } else {
    currencyBuilder.difficultyWindow(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    currencyBuilder.difficultyWindowV1(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    currencyBuilder.difficultyWindowV2(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
  }
  currencyBuilder.maxBlockSizeGrowthSpeedDenominator(365 * 24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
  currencyBuilder.lockedTxAllowedDeltaSeconds(command_line::get_arg(vm, arg_DIFFICULTY_TARGET) * CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);  
  if (command_line::has_arg(vm, arg_UPGRADE_HEIGHT_V2) && command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V2) != 0)
  {
    currencyBuilder.upgradeHeightV2(command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V2));
  }
  if (command_line::has_arg(vm, arg_UPGRADE_HEIGHT_V3) && command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V3) != 0)
  {
    currencyBuilder.upgradeHeightV3(command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V3));
  }
  currencyBuilder.difficultyLag(command_line::get_arg(vm, arg_DIFFICULTY_LAG));
  currencyBuilder.difficultyCut(command_line::get_arg(vm, arg_DIFFICULTY_CUT));
  if (command_line::has_arg(vm, arg_DIFFICULTY_WINDOW_V1) && command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V1) != 0)
  {
    currencyBuilder.difficultyWindowV1(command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V1));
  }
  if (command_line::has_arg(vm, arg_DIFFICULTY_WINDOW_V2) && command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V2) != 0)
  {
    currencyBuilder.difficultyWindowV2(command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V2));
  }
  currencyBuilder.difficultyLagV1(command_line::get_arg(vm, arg_DIFFICULTY_LAG_V1));
  currencyBuilder.difficultyLagV2(command_line::get_arg(vm, arg_DIFFICULTY_LAG_V2));
  currencyBuilder.difficultyCutV1(command_line::get_arg(vm, arg_DIFFICULTY_CUT_V1));
  currencyBuilder.difficultyCutV2(command_line::get_arg(vm, arg_DIFFICULTY_CUT_V2));
  currencyBuilder.genesisBlockReward(command_line::get_arg(vm, arg_GENESIS_BLOCK_REWARD));
  CryptoNote::Transaction tx = currencyBuilder.generateGenesisTransaction(targets);
    currencyBuilder.cryptonoteName(command_line::get_arg(vm, arg_CRYPTONOTE_NAME));
      std::string tx_hex = Common::toHex(CryptoNote::toBinaryArray(tx));
      std::cout << "Modify this line into your coin configuration file as is: " << std::endl;
  std::cout << "GENESIS_COINBASE_TX_HEX=" << tx_hex << std::endl;
  }
  return;
}

JsonValue buildLoggerConfiguration(Level level, const std::string& logfile) {
  JsonValue loggerConfiguration(JsonValue::OBJECT);
  loggerConfiguration.insert("globalLevel", static_cast<int64_t>(level));

  JsonValue& cfgLoggers = loggerConfiguration.insert("loggers", JsonValue::ARRAY);

  JsonValue& fileLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
  fileLogger.insert("type", "file");
  fileLogger.insert("filename", logfile);
  fileLogger.insert("level", static_cast<int64_t>(TRACE));

  JsonValue& consoleLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
  consoleLogger.insert("type", "console");
  consoleLogger.insert("level", static_cast<int64_t>(TRACE));
  consoleLogger.insert("pattern", "%D %T %L ");

  return loggerConfiguration;
}

int main(int argc, char* argv[])
{

#ifdef WIN32
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

  LoggerManager logManager;
  LoggerRef logger(logManager, "daemon");

  try {
    po::options_description desc_cmd_only("Command line options");
    po::options_description desc_cmd_sett("Command line options and settings options");

    command_line::add_arg(desc_cmd_only, command_line::arg_help);
    command_line::add_arg(desc_cmd_only, command_line::arg_version);
    command_line::add_arg(desc_cmd_only, arg_os_version);
    // tools::get_default_data_dir() can't be called during static initialization
    command_line::add_arg(desc_cmd_only, command_line::arg_data_dir, Tools::getDefaultDataDirectory());
    command_line::add_arg(desc_cmd_only, arg_config_file);

    command_line::add_arg(desc_cmd_sett, arg_log_file);
    command_line::add_arg(desc_cmd_sett, arg_log_level);
    command_line::add_arg(desc_cmd_sett, arg_console);
    command_line::add_arg(desc_cmd_sett, arg_set_fee_address);
    command_line::add_arg(desc_cmd_sett, arg_testnet_on);
    command_line::add_arg(desc_cmd_sett, arg_GENESIS_COINBASE_TX_HEX);
    command_line::add_arg(desc_cmd_sett, arg_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);
    command_line::add_arg(desc_cmd_sett, arg_MONEY_SUPPLY);
    command_line::add_arg(desc_cmd_sett, arg_EMISSION_SPEED_FACTOR);
    command_line::add_arg(desc_cmd_sett, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
    command_line::add_arg(desc_cmd_sett, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1);
    command_line::add_arg(desc_cmd_sett, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2);
    command_line::add_arg(desc_cmd_sett, arg_CRYPTONOTE_DISPLAY_DECIMAL_POINT);
    command_line::add_arg(desc_cmd_sett, arg_MINIMUM_FEE);
    command_line::add_arg(desc_cmd_sett, arg_DEFAULT_DUST_THRESHOLD);
    command_line::add_arg(desc_cmd_sett, arg_DIFFICULTY_TARGET);
    command_line::add_arg(desc_cmd_sett, arg_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);
    command_line::add_arg(desc_cmd_sett, arg_MAX_BLOCK_SIZE_INITIAL);
    command_line::add_arg(desc_cmd_sett, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
    command_line::add_arg(desc_cmd_sett, arg_UPGRADE_HEIGHT_V2);
    command_line::add_arg(desc_cmd_sett, arg_UPGRADE_HEIGHT_V3);
    command_line::add_arg(desc_cmd_sett, arg_KEY_IMAGE_CHECKING_BLOCK_INDEX);
    command_line::add_arg(desc_cmd_sett, arg_DIFFICULTY_WINDOW_V1);
    command_line::add_arg(desc_cmd_sett, arg_DIFFICULTY_WINDOW_V2);
    command_line::add_arg(desc_cmd_sett, arg_DIFFICULTY_CUT_V1);
    command_line::add_arg(desc_cmd_sett, arg_DIFFICULTY_CUT_V2);
    command_line::add_arg(desc_cmd_sett, arg_DIFFICULTY_LAG_V1);
    command_line::add_arg(desc_cmd_sett, arg_DIFFICULTY_LAG_V2);
    command_line::add_arg(desc_cmd_sett, arg_DIFFICULTY_WINDOW);
    command_line::add_arg(desc_cmd_sett, arg_DIFFICULTY_CUT);
    command_line::add_arg(desc_cmd_sett, arg_DIFFICULTY_LAG);
    command_line::add_arg(desc_cmd_sett, arg_CRYPTONOTE_NAME);
    command_line::add_arg(desc_cmd_sett, arg_CHECKPOINT);
    command_line::add_arg(desc_cmd_sett, arg_BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX);
    command_line::add_arg(desc_cmd_sett, arg_ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX);
    command_line::add_arg(desc_cmd_sett, arg_ZAWY_LWMA_DIFFICULTY_LAST_BLOCK);
    command_line::add_arg(desc_cmd_sett, arg_ZAWY_LWMA_DIFFICULTY_N);
    command_line::add_arg(desc_cmd_sett, arg_ZAWY_DIFFICULTY_BLOCK_INDEX);
    command_line::add_arg(desc_cmd_sett, arg_ZAWY_DIFFICULTY_LAST_BLOCK);
    command_line::add_arg(desc_cmd_sett, arg_GENESIS_BLOCK_REWARD);
    command_line::add_arg(desc_cmd_sett, arg_CRYPTONOTE_COIN_VERSION);
    command_line::add_arg(desc_cmd_sett, arg_TAIL_EMISSION_REWARD);
    command_line::add_arg(desc_cmd_sett, arg_KILL_HEIGHT);
    command_line::add_arg(desc_cmd_sett, arg_MANDATORY_TRANSACTION);
    command_line::add_arg(desc_cmd_sett, arg_MIXIN_START_HEIGHT);
    command_line::add_arg(desc_cmd_sett, arg_MIN_MIXIN);
    command_line::add_arg(desc_cmd_sett, arg_MANDATORY_MIXIN_BLOCK_VERSION);
command_line::add_arg(desc_cmd_sett, arg_enable_cors);
    command_line::add_arg(desc_cmd_sett, arg_blockexplorer_on);
command_line::add_arg(desc_cmd_sett, arg_print_genesis_tx);
  command_line::add_arg(desc_cmd_sett, arg_genesis_block_reward_address);

    RpcServerConfig::initOptions(desc_cmd_sett);
    NetNodeConfig::initOptions(desc_cmd_sett);
    DataBaseConfig::initOptions(desc_cmd_sett);

    po::options_description desc_options("Allowed options");
    desc_options.add(desc_cmd_only).add(desc_cmd_sett);

    po::variables_map vm;
    boost::filesystem::path data_dir_path;
    bool r = command_line::handle_error_helper(desc_options, [&]()
    {
      po::store(po::parse_command_line(argc, argv, desc_options), vm);

      if (command_line::get_arg(vm, command_line::arg_help))
      {
        std::cout << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG << ENDL << ENDL;
        std::cout << desc_options << std::endl;
        return false;
      }

      std::string data_dir = command_line::get_arg(vm, command_line::arg_data_dir);
      std::string config = command_line::get_arg(vm, arg_config_file);

      data_dir_path = data_dir;
      boost::filesystem::path config_path(config);
      if (!config_path.has_parent_path()) {
        config_path = data_dir_path / config_path;
      }

      boost::system::error_code ec;
      if (boost::filesystem::exists(config_path, ec)) {
        std::cout << "Success: Configuration file openned: " << config_path << std::endl;
        po::store(po::parse_config_file<char>(config_path.string<std::string>().c_str(), desc_cmd_sett, true), vm);
      }
      else
      {
        std::cout << "Configuration error: Cannot open configuration file" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "Usage:" << std::endl;
        std::cout << "Windows:   forknoted.exe --config-file configs/dashcoin.conf" << std::endl;
        std::cout << "Linux/Mac:   ./forknoted --config-file configs/dashcoin.conf" << std::endl;
        return false;
      }
      po::notify(vm);
      if (command_line::get_arg(vm, command_line::arg_data_dir) == Tools::getDefaultDataDirectory() && command_line::has_arg(vm, arg_CRYPTONOTE_NAME) && !command_line::get_arg(vm, arg_CRYPTONOTE_NAME).empty()) {
        boost::replace_all(data_dir, CryptoNote::CRYPTONOTE_NAME, command_line::get_arg(vm, arg_CRYPTONOTE_NAME));
      }
      data_dir_path = data_dir;
      if (command_line::get_arg(vm, arg_print_genesis_tx)) {
        print_genesis_tx_hex(vm, logManager);
        return false;
      }
      return true;
    });

    if (!r)
      return 1;
  
    auto modulePath = Common::NativePathToGeneric(argv[0]);
    auto cfgLogFile = Common::NativePathToGeneric(command_line::get_arg(vm, arg_log_file));

    if (cfgLogFile.empty()) {
      cfgLogFile = Common::ReplaceExtenstion(modulePath, ".log");
    } else {
      if (!Common::HasParentPath(cfgLogFile)) {
        cfgLogFile = Common::CombinePath(Common::GetPathDirectory(modulePath), cfgLogFile);
      }
    }

    Level cfgLogLevel = static_cast<Level>(static_cast<int>(Logging::ERROR) + command_line::get_arg(vm, arg_log_level));

    // configure logging
    logManager.configure(buildLoggerConfiguration(cfgLogLevel, cfgLogFile));

    logger(INFO) << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG;

    if (command_line_preprocessor(vm, logger)) {
      return 0;
    }

    logger(INFO) << "Module folder: " << argv[0];

    bool testnet_mode = command_line::get_arg(vm, arg_testnet_on);
    if (testnet_mode) {
      logger(INFO) << "Starting in testnet mode!";
    }

    //create objects and link them
    CryptoNote::CurrencyBuilder currencyBuilder(logManager);
    currencyBuilder.cryptonoteName(command_line::get_arg(vm, arg_CRYPTONOTE_NAME));
  currencyBuilder.minMixin(command_line::get_arg(vm, arg_MIN_MIXIN));
//uint8_t recognized as char
  if (command_line::get_arg(vm, arg_MANDATORY_MIXIN_BLOCK_VERSION) == 0) {
    currencyBuilder.mandatoryMixinBlockVersion(command_line::get_arg(vm, arg_MANDATORY_MIXIN_BLOCK_VERSION));
  } else {
    currencyBuilder.mandatoryMixinBlockVersion(command_line::get_arg(vm, arg_MANDATORY_MIXIN_BLOCK_VERSION) - '0');
  }
  currencyBuilder.mandatoryTransaction(command_line::get_arg(vm, arg_MANDATORY_TRANSACTION));
    currencyBuilder.genesisCoinbaseTxHex(command_line::get_arg(vm, arg_GENESIS_COINBASE_TX_HEX));
    currencyBuilder.publicAddressBase58Prefix(command_line::get_arg(vm, arg_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX));
    currencyBuilder.moneySupply(command_line::get_arg(vm, arg_MONEY_SUPPLY));
    currencyBuilder.emissionSpeedFactor(command_line::get_arg(vm, arg_EMISSION_SPEED_FACTOR));
    currencyBuilder.blockGrantedFullRewardZone(command_line::get_arg(vm, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE));
    currencyBuilder.blockGrantedFullRewardZoneV1(command_line::get_arg(vm, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1));
    currencyBuilder.blockGrantedFullRewardZoneV2(command_line::get_arg(vm, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2));
    currencyBuilder.numberOfDecimalPlaces(command_line::get_arg(vm, arg_CRYPTONOTE_DISPLAY_DECIMAL_POINT));
    currencyBuilder.mininumFee(command_line::get_arg(vm, arg_MINIMUM_FEE));
    currencyBuilder.defaultDustThreshold(command_line::get_arg(vm, arg_DEFAULT_DUST_THRESHOLD));
    currencyBuilder.difficultyTarget(command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    currencyBuilder.minedMoneyUnlockWindow(command_line::get_arg(vm, arg_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW));
    currencyBuilder.maxBlockSizeInitial(command_line::get_arg(vm, arg_MAX_BLOCK_SIZE_INITIAL));
    if (command_line::has_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY) && command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY) != 0)
    {
      currencyBuilder.expectedNumberOfBlocksPerDay(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
      currencyBuilder.difficultyWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    currencyBuilder.difficultyWindowV1(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    currencyBuilder.difficultyWindowV2(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
      currencyBuilder.upgradeVotingWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
      currencyBuilder.upgradeWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    } else {
      currencyBuilder.expectedNumberOfBlocksPerDay(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
      currencyBuilder.difficultyWindow(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    currencyBuilder.difficultyWindowV1(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    currencyBuilder.difficultyWindowV2(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    }
    currencyBuilder.maxBlockSizeGrowthSpeedDenominator(365 * 24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
    currencyBuilder.lockedTxAllowedDeltaSeconds(command_line::get_arg(vm, arg_DIFFICULTY_TARGET) * CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);  
    if (command_line::has_arg(vm, arg_UPGRADE_HEIGHT_V2) && command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V2) != 0)
    {
      currencyBuilder.upgradeHeightV2(command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V2));
    }
    if (command_line::has_arg(vm, arg_UPGRADE_HEIGHT_V3) && command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V3) != 0)
    {
      currencyBuilder.upgradeHeightV3(command_line::get_arg(vm, arg_UPGRADE_HEIGHT_V3));
    }
    if (command_line::has_arg(vm, arg_KEY_IMAGE_CHECKING_BLOCK_INDEX) && command_line::get_arg(vm, arg_KEY_IMAGE_CHECKING_BLOCK_INDEX) != 0)
    {
      currencyBuilder.keyImageCheckingBlockIndex(command_line::get_arg(vm, arg_KEY_IMAGE_CHECKING_BLOCK_INDEX));
    }
    if (command_line::has_arg(vm, arg_DIFFICULTY_WINDOW) && command_line::get_arg(vm, arg_DIFFICULTY_WINDOW) != 0)
    {
      currencyBuilder.difficultyWindow(command_line::get_arg(vm, arg_DIFFICULTY_WINDOW));
    }
    currencyBuilder.difficultyLag(command_line::get_arg(vm, arg_DIFFICULTY_LAG));
    currencyBuilder.difficultyCut(command_line::get_arg(vm, arg_DIFFICULTY_CUT));
  if (command_line::has_arg(vm, arg_DIFFICULTY_WINDOW_V1) && command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V1) != 0)
  {
    currencyBuilder.difficultyWindowV1(command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V1));
  }
  if (command_line::has_arg(vm, arg_DIFFICULTY_WINDOW_V2) && command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V2) != 0)
  {
    currencyBuilder.difficultyWindowV2(command_line::get_arg(vm, arg_DIFFICULTY_WINDOW_V2));
  }
  currencyBuilder.difficultyLagV1(command_line::get_arg(vm, arg_DIFFICULTY_LAG_V1));
  currencyBuilder.difficultyLagV2(command_line::get_arg(vm, arg_DIFFICULTY_LAG_V2));
  currencyBuilder.difficultyCutV1(command_line::get_arg(vm, arg_DIFFICULTY_CUT_V1));
  currencyBuilder.difficultyCutV2(command_line::get_arg(vm, arg_DIFFICULTY_CUT_V2));
bool blockexplorer_mode = command_line::get_arg(vm, arg_blockexplorer_on);
currencyBuilder.isBlockexplorer(blockexplorer_mode);
    currencyBuilder.mixinStartHeight(command_line::get_arg(vm, arg_MIXIN_START_HEIGHT));
    currencyBuilder.killHeight(command_line::get_arg(vm, arg_KILL_HEIGHT));
    currencyBuilder.tailEmissionReward(command_line::get_arg(vm, arg_TAIL_EMISSION_REWARD));
    currencyBuilder.cryptonoteCoinVersion(command_line::get_arg(vm, arg_CRYPTONOTE_COIN_VERSION));
    currencyBuilder.genesisBlockReward(command_line::get_arg(vm, arg_GENESIS_BLOCK_REWARD));
    currencyBuilder.zawyDifficultyBlockIndex(command_line::get_arg(vm, arg_ZAWY_DIFFICULTY_BLOCK_INDEX));
    currencyBuilder.zawyDifficultyLastBlock(command_line::get_arg(vm, arg_ZAWY_DIFFICULTY_LAST_BLOCK));
    currencyBuilder.zawyLWMADifficultyBlockIndex(command_line::get_arg(vm, arg_ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX));
    currencyBuilder.zawyLWMADifficultyLastBlock(command_line::get_arg(vm, arg_ZAWY_LWMA_DIFFICULTY_LAST_BLOCK));
    currencyBuilder.zawyLWMADifficultyN(command_line::get_arg(vm, arg_ZAWY_LWMA_DIFFICULTY_N));
    currencyBuilder.buggedZawyDifficultyBlockIndex(command_line::get_arg(vm, arg_BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX));
    currencyBuilder.testnet(testnet_mode);
    try {
      currencyBuilder.currency();
    } catch (std::exception&) {
      std::cout << "GENESIS_COINBASE_TX_HEX constant has an incorrect value. Please launch: " << CryptoNote::CRYPTONOTE_NAME << "d --" << arg_print_genesis_tx.name;
      return 1;
    }
    CryptoNote::Currency currency = currencyBuilder.currency();

    CryptoNote::Checkpoints checkpoints(logManager);
std::vector<CryptoNote::CheckpointData> checkpoint_input;
std::vector<std::string> checkpoint_args = command_line::get_arg(vm, arg_CHECKPOINT);
std::vector<std::string> checkpoint_blockIds;
if (command_line::has_arg(vm, arg_CHECKPOINT) && checkpoint_args.size() != 0)
{
  for(const std::string& str: checkpoint_args) {
    std::string::size_type p = str.find(':');
    if(p != std::string::npos)
    {
      uint32_t checkpoint_height = std::stoull(str.substr(0, p));
      checkpoint_blockIds.push_back(str.substr(p+1, str.size()));
      checkpoint_input.push_back({ checkpoint_height, checkpoint_blockIds.back().c_str() });
    }
  }
}
else
{
  if (command_line::get_arg(vm, arg_CRYPTONOTE_NAME) == "bytecoin") {
      checkpoint_input = CryptoNote::CHECKPOINTS;
  }
}
    if (!testnet_mode) {
for (const auto& cp : checkpoint_input) {
        checkpoints.addCheckpoint(cp.index, cp.blockId);
      }
    }
    
    NetNodeConfig netNodeConfig;
    netNodeConfig.init(vm);
    netNodeConfig.setTestnet(testnet_mode);
    netNodeConfig.setConfigFolder(data_dir_path.string());

    RpcServerConfig rpcConfig;
    rpcConfig.init(vm);

    DataBaseConfig dbConfig;
    dbConfig.init(vm);

    dbConfig.setDataDir(data_dir_path.string());
    if (dbConfig.isConfigFolderDefaulted()) {
      if (!Tools::create_directories_if_necessary(dbConfig.getDataDir())) {
        throw std::runtime_error("Can't create directory: " + dbConfig.getDataDir());
      }
    } else {
      if (!Tools::directoryExists(dbConfig.getDataDir())) {
        throw std::runtime_error("Directory does not exist: " + dbConfig.getDataDir());
      }
    }

    RocksDBWrapper database(logManager);
    database.init(dbConfig);
    Tools::ScopeExit dbShutdownOnExit([&database] () { database.shutdown(); });

    if (!DatabaseBlockchainCache::checkDBSchemeVersion(database, logManager))
    {
      dbShutdownOnExit.cancel();
      database.shutdown();

      database.destoy(dbConfig);

      database.init(dbConfig);
      dbShutdownOnExit.resume();
    }

    System::Dispatcher dispatcher;
    logger(INFO) << "Initializing core...";
    CryptoNote::Core ccore(
      currency,
      logManager,
      std::move(checkpoints),
      dispatcher,
      std::unique_ptr<IBlockchainCacheFactory>(new DatabaseBlockchainCacheFactory(database, logger.getLogger())),
      createSwappedMainChainStorage(data_dir_path.string(), currency));

    ccore.load();
    logger(INFO) << "Core initialized OK";

    CryptoNote::CryptoNoteProtocolHandler cprotocol(currency, dispatcher, ccore, nullptr, logManager);
    CryptoNote::NodeServer p2psrv(dispatcher, cprotocol, logManager);
    CryptoNote::RpcServer rpcServer(dispatcher, logManager, ccore, p2psrv, cprotocol);

    cprotocol.set_p2p_endpoint(&p2psrv);
    DaemonCommandsHandler dch(ccore, p2psrv, logManager);
    logger(INFO) << "Initializing p2p server...";
    if (!p2psrv.init(netNodeConfig)) {
      logger(ERROR, BRIGHT_RED) << "Failed to initialize p2p server.";
      return 1;
    }

    logger(INFO) << "P2p server initialized OK";

    if (!command_line::has_arg(vm, arg_console)) {
      dch.start_handling();
    }

    logger(INFO) << "Starting core rpc server on address " << rpcConfig.getBindAddress();
    rpcServer.start(rpcConfig.bindIp, rpcConfig.bindPort);
  rpcServer.setFeeAddress(command_line::get_arg(vm, arg_set_fee_address));
rpcServer.enableCors(command_line::get_arg(vm, arg_enable_cors));
    logger(INFO) << "Core rpc server started ok";

    Tools::SignalHandler::install([&dch, &p2psrv] {
      dch.stop_handling();
      p2psrv.sendStopSignal();
    });

    logger(INFO) << "Starting p2p net loop...";
    p2psrv.run();
    logger(INFO) << "p2p net loop stopped";

    dch.stop_handling();

    //stop components
    logger(INFO) << "Stopping core rpc server...";
    rpcServer.stop();

    //deinitialize components
    logger(INFO) << "Deinitializing p2p...";
    p2psrv.deinit();

    cprotocol.set_p2p_endpoint(nullptr);
    ccore.save();

  } catch (const std::exception& e) {
    logger(ERROR, BRIGHT_RED) << "Exception: " << e.what();
    return 1;
  }

  logger(INFO) << "Node stopped.";
  return 0;
}

bool command_line_preprocessor(const boost::program_options::variables_map &vm, LoggerRef &logger) {
  bool exit = false;

  if (command_line::get_arg(vm, command_line::arg_version)) {
    std::cout << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG << ENDL;
    exit = true;
  }
  if (command_line::get_arg(vm, arg_os_version)) {
    std::cout << "OS: " << Tools::get_os_version_string() << ENDL;
    exit = true;
  }

  if (exit) {
    return true;
  }

  return false;
}
