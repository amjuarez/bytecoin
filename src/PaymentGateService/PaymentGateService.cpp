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

#include "PaymentGateService.h"

#include <future>

#include "Common/SignalHandler.h"
#include "Common/Util.h"
#include "InProcessNode/InProcessNode.h"
#include "Logging/LoggerRef.h"
#include "PaymentGate/PaymentServiceJsonRpcServer.h"

#include "Common/ScopeExit.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/DatabaseBlockchainCache.h"
#include "CryptoNoteCore/DatabaseBlockchainCacheFactory.h"
#include "CryptoNoteCore/DataBaseConfig.h"
#include "CryptoNoteCore/MainChainStorage.h"
#include "CryptoNoteCore/RocksDBWrapper.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "P2p/NetNode.h"
#include <System/Context.h>
#include "Wallet/WalletGreen.h"

#ifdef ERROR
#undef ERROR
#endif

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

using namespace PaymentService;

void changeDirectory(const std::string& path) {
  if (chdir(path.c_str())) {
    throw std::runtime_error("Couldn't change directory to \'" + path + "\': " + strerror(errno));
  }
}

void stopSignalHandler(PaymentGateService* pg) {
  pg->stop();
}

PaymentGateService::PaymentGateService() :
  dispatcher(nullptr),
  stopEvent(nullptr),
  config(),
  service(nullptr),
  logger(),
  currencyBuilder(logger),
  fileLogger(Logging::TRACE),
  consoleLogger(Logging::INFO) {
  consoleLogger.setPattern("%D %T %L ");
  fileLogger.setPattern("%D %T %L ");
}

bool PaymentGateService::init(int argc, char** argv) {
  if (!config.init(argc, argv)) {
    return false;
  }

  logger.setMaxLevel(static_cast<Logging::Level>(config.gateConfiguration.logLevel));
  logger.setPattern("%D %T %L ");
  logger.addLogger(consoleLogger);

  Logging::LoggerRef log(logger, "main");

  currencyBuilder.genesisCoinbaseTxHex(config.coinBaseConfig.GENESIS_COINBASE_TX_HEX);
  currencyBuilder.publicAddressBase58Prefix(config.coinBaseConfig.CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);
  currencyBuilder.moneySupply(config.coinBaseConfig.MONEY_SUPPLY);
  currencyBuilder.buggedZawyDifficultyBlockIndex(config.coinBaseConfig.BUGGED_ZAWY_DIFFICULTY_BLOCK_INDEX);
  currencyBuilder.zawyLWMADifficultyBlockIndex(config.coinBaseConfig.ZAWY_LWMA_DIFFICULTY_BLOCK_INDEX);
  currencyBuilder.zawyLWMADifficultyLastBlock(config.coinBaseConfig.ZAWY_LWMA_DIFFICULTY_LAST_BLOCK);
  currencyBuilder.zawyLWMADifficultyN(config.coinBaseConfig.ZAWY_LWMA_DIFFICULTY_N);
  currencyBuilder.zawyDifficultyBlockIndex(config.coinBaseConfig.ZAWY_DIFFICULTY_BLOCK_INDEX);
  currencyBuilder.zawyDifficultyLastBlock(config.coinBaseConfig.ZAWY_DIFFICULTY_LAST_BLOCK);
  currencyBuilder.genesisBlockReward(config.coinBaseConfig.GENESIS_BLOCK_REWARD);
  currencyBuilder.cryptonoteCoinVersion(config.coinBaseConfig.CRYPTONOTE_COIN_VERSION);
  currencyBuilder.tailEmissionReward(config.coinBaseConfig.TAIL_EMISSION_REWARD);
  currencyBuilder.killHeight(config.coinBaseConfig.KILL_HEIGHT);
  currencyBuilder.mandatoryTransaction(config.coinBaseConfig.MANDATORY_TRANSACTION);
  currencyBuilder.mixinStartHeight(config.coinBaseConfig.MIXIN_START_HEIGHT);
  currencyBuilder.minMixin(config.coinBaseConfig.MIN_MIXIN);
//uint8_t recognized as char
  if (config.coinBaseConfig.MANDATORY_MIXIN_BLOCK_VERSION == 0) {
    currencyBuilder.mandatoryMixinBlockVersion(config.coinBaseConfig.MANDATORY_MIXIN_BLOCK_VERSION);
  } else {
    currencyBuilder.mandatoryMixinBlockVersion(config.coinBaseConfig.MANDATORY_MIXIN_BLOCK_VERSION - '0');
  }
  currencyBuilder.emissionSpeedFactor(config.coinBaseConfig.EMISSION_SPEED_FACTOR);
  currencyBuilder.blockGrantedFullRewardZone(config.coinBaseConfig.CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
  currencyBuilder.blockGrantedFullRewardZoneV1(config.coinBaseConfig.CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1);
  currencyBuilder.blockGrantedFullRewardZoneV2(config.coinBaseConfig.CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2);
  currencyBuilder.numberOfDecimalPlaces(config.coinBaseConfig.CRYPTONOTE_DISPLAY_DECIMAL_POINT);
  currencyBuilder.mininumFee(config.coinBaseConfig.MINIMUM_FEE);
  currencyBuilder.defaultDustThreshold(config.coinBaseConfig.DEFAULT_DUST_THRESHOLD);
  currencyBuilder.difficultyTarget(config.coinBaseConfig.DIFFICULTY_TARGET);
  currencyBuilder.minedMoneyUnlockWindow(config.coinBaseConfig.CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);
  currencyBuilder.maxBlockSizeInitial(config.coinBaseConfig.MAX_BLOCK_SIZE_INITIAL);
  if (config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY && config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY != 0)
  {
    currencyBuilder.expectedNumberOfBlocksPerDay(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
    currencyBuilder.difficultyWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
    currencyBuilder.difficultyWindowV1(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
    currencyBuilder.difficultyWindowV2(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
    currencyBuilder.upgradeVotingWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
    currencyBuilder.upgradeWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
  } else {
    currencyBuilder.expectedNumberOfBlocksPerDay(24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
    currencyBuilder.difficultyWindow(24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
    currencyBuilder.difficultyWindowV1(24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
    currencyBuilder.difficultyWindowV2(24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
  }
  currencyBuilder.maxBlockSizeGrowthSpeedDenominator(365 * 24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
  currencyBuilder.lockedTxAllowedDeltaSeconds(config.coinBaseConfig.DIFFICULTY_TARGET * CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);
  if (config.coinBaseConfig.UPGRADE_HEIGHT_V2 && config.coinBaseConfig.UPGRADE_HEIGHT_V2 != 0)
  {
    currencyBuilder.upgradeHeightV2(config.coinBaseConfig.UPGRADE_HEIGHT_V2);
  }
  if (config.coinBaseConfig.UPGRADE_HEIGHT_V3 && config.coinBaseConfig.UPGRADE_HEIGHT_V3 != 0)
  {
    currencyBuilder.upgradeHeightV3(config.coinBaseConfig.UPGRADE_HEIGHT_V3);
  }
  if (config.coinBaseConfig.KEY_IMAGE_CHECKING_BLOCK_INDEX && config.coinBaseConfig.KEY_IMAGE_CHECKING_BLOCK_INDEX != 0)
  {
    currencyBuilder.keyImageCheckingBlockIndex(config.coinBaseConfig.KEY_IMAGE_CHECKING_BLOCK_INDEX);
  }
  if (config.coinBaseConfig.DIFFICULTY_WINDOW && config.coinBaseConfig.DIFFICULTY_WINDOW != 0)
  {
    currencyBuilder.difficultyWindow(config.coinBaseConfig.DIFFICULTY_WINDOW);
  }
  currencyBuilder.difficultyLag(config.coinBaseConfig.DIFFICULTY_LAG);
if (config.coinBaseConfig.MAX_TRANSACTION_SIZE_LIMIT == 0) {
  uint64_t maxTxSizeLimit = config.coinBaseConfig.CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE * 110 / 100 - CryptoNote::parameters::CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE;
  currencyBuilder.maxTransactionSizeLimit(maxTxSizeLimit);
  currencyBuilder.fusionTxMaxSize(maxTxSizeLimit * 30 / 100);
} else {
  currencyBuilder.maxTransactionSizeLimit(config.coinBaseConfig.MAX_TRANSACTION_SIZE_LIMIT);
  currencyBuilder.fusionTxMaxSize(config.coinBaseConfig.MAX_TRANSACTION_SIZE_LIMIT * 30 / 100);
}
  currencyBuilder.difficultyCut(config.coinBaseConfig.DIFFICULTY_CUT);
  if (config.coinBaseConfig.DIFFICULTY_WINDOW_V1 && config.coinBaseConfig.DIFFICULTY_WINDOW_V1 != 0)
  {
    currencyBuilder.difficultyWindowV1(config.coinBaseConfig.DIFFICULTY_WINDOW_V1);
  }
  if (config.coinBaseConfig.DIFFICULTY_WINDOW_V2 && config.coinBaseConfig.DIFFICULTY_WINDOW_V2 != 0)
  {
    currencyBuilder.difficultyWindowV2(config.coinBaseConfig.DIFFICULTY_WINDOW_V2);
  }
  currencyBuilder.difficultyLagV1(config.coinBaseConfig.DIFFICULTY_LAG_V1);
  currencyBuilder.difficultyLagV2(config.coinBaseConfig.DIFFICULTY_LAG_V2);
  currencyBuilder.difficultyCutV1(config.coinBaseConfig.DIFFICULTY_CUT_V1);
  currencyBuilder.difficultyCutV2(config.coinBaseConfig.DIFFICULTY_CUT_V2);
  if (config.gateConfiguration.testnet) {
    log(Logging::INFO) << "Starting in testnet mode";
    currencyBuilder.testnet(true);
  }

  if (!config.gateConfiguration.serverRoot.empty()) {
    changeDirectory(config.gateConfiguration.serverRoot);
    log(Logging::INFO) << "Current working directory now is " << config.gateConfiguration.serverRoot;
  }

  fileStream.open(config.gateConfiguration.logFile, std::ofstream::app);

  if (!fileStream) {
    throw std::runtime_error("Couldn't open log file");
  }

  fileLogger.attachToStream(fileStream);
  logger.addLogger(fileLogger);

  return true;
}

WalletConfiguration PaymentGateService::getWalletConfig() const {
  return WalletConfiguration{
    config.gateConfiguration.containerFile,
    config.gateConfiguration.containerPassword,
    config.gateConfiguration.syncFromZero
  };
}

const CryptoNote::Currency PaymentGateService::getCurrency() {
  return currencyBuilder.currency();
}

void PaymentGateService::run() {
  
  System::Dispatcher localDispatcher;
  System::Event localStopEvent(localDispatcher);

  this->dispatcher = &localDispatcher;
  this->stopEvent = &localStopEvent;

  Tools::SignalHandler::install(std::bind(&stopSignalHandler, this));

  Logging::LoggerRef log(logger, "run");

  if (config.startInprocess) {
    runInProcess(log);
  } else {
    runRpcProxy(log);
  }

  this->dispatcher = nullptr;
  this->stopEvent = nullptr;
}

void PaymentGateService::stop() {
  Logging::LoggerRef log(logger, "stop");

  log(Logging::INFO, Logging::BRIGHT_WHITE) << "Stop signal caught";

  if (dispatcher != nullptr) {
    dispatcher->remoteSpawn([&]() {
      if (stopEvent != nullptr) {
        stopEvent->set();
      }
    });
  }
}

void PaymentGateService::runInProcess(Logging::LoggerRef& log) {
  log(Logging::INFO) << "Starting Payment Gate with local node";

      std::string data_dir = config.dataDir;
      if (config.dataDir == Tools::getDefaultDataDirectory() && !config.coinBaseConfig.CRYPTONOTE_NAME.empty()) {
        boost::replace_all(data_dir, CryptoNote::CRYPTONOTE_NAME, config.coinBaseConfig.CRYPTONOTE_NAME);
      }
  CryptoNote::DataBaseConfig dbConfig;

  //TODO: make command line options
  dbConfig.setConfigFolderDefaulted(true);
dbConfig.setDataDir(data_dir);
  dbConfig.setMaxOpenFiles(100);
  dbConfig.setReadCacheSize(128*1024*1024);
  dbConfig.setWriteBufferSize(128*1024*1024);
  dbConfig.setTestnet(false);
  dbConfig.setBackgroundThreadsCount(2);

  if (dbConfig.isConfigFolderDefaulted()) {
    if (!Tools::create_directories_if_necessary(dbConfig.getDataDir())) {
      throw std::runtime_error("Can't create directory: " + dbConfig.getDataDir());
    }
  } else {
    if (!Tools::directoryExists(dbConfig.getDataDir())) {
      throw std::runtime_error("Directory does not exist: " + dbConfig.getDataDir());
    }
  }

  CryptoNote::RocksDBWrapper database(logger);
  database.init(dbConfig);
  Tools::ScopeExit dbShutdownOnExit([&database] () { database.shutdown(); });

  if (!CryptoNote::DatabaseBlockchainCache::checkDBSchemeVersion(database, logger))
  {
    dbShutdownOnExit.cancel();
    database.shutdown();

    database.destoy(dbConfig);

    database.init(dbConfig);
    dbShutdownOnExit.resume();
  }

  CryptoNote::Currency currency = currencyBuilder.currency();

  log(Logging::INFO) << "initializing core";

  CryptoNote::Core core(
    currency,
    logger,
    CryptoNote::Checkpoints(logger),
    *dispatcher,
    std::unique_ptr<CryptoNote::IBlockchainCacheFactory>(new CryptoNote::DatabaseBlockchainCacheFactory(database, log.getLogger())),
    CryptoNote::createSwappedMainChainStorage(dbConfig.getDataDir(), currency));

  core.load();

  CryptoNote::CryptoNoteProtocolHandler protocol(currency, *dispatcher, core, nullptr, logger);
  CryptoNote::NodeServer p2pNode(*dispatcher, protocol, logger);

  protocol.set_p2p_endpoint(&p2pNode);

  log(Logging::INFO) << "initializing p2pNode";
  if (!p2pNode.init(config.netNodeConfig)) {
    throw std::runtime_error("Failed to init p2pNode");
  }

  std::unique_ptr<CryptoNote::INode> node(new CryptoNote::InProcessNode(core, protocol, *dispatcher));

  std::error_code nodeInitStatus;
  node->init([&nodeInitStatus](std::error_code ec) {
    nodeInitStatus = ec;
  });

  if (nodeInitStatus) {
    log(Logging::WARNING, Logging::YELLOW) << "Failed to init node: " << nodeInitStatus.message();
    throw std::system_error(nodeInitStatus);
  } else {
    log(Logging::INFO) << "node is inited successfully";
  }

  log(Logging::INFO) << "Spawning p2p server";

  System::Event p2pStarted(*dispatcher);
  
  System::Context<> context(*dispatcher, [&]() {
    p2pStarted.set();
    p2pNode.run();
  });

  p2pStarted.wait();

  runWalletService(currency, *node);

  p2pNode.sendStopSignal();
  context.get();
  node->shutdown();
  p2pNode.deinit(); 
}

void PaymentGateService::runRpcProxy(Logging::LoggerRef& log) {
  log(Logging::INFO) << "Starting Payment Gate with remote node";
  CryptoNote::Currency currency = currencyBuilder.currency();
  
  std::unique_ptr<CryptoNote::INode> node(
    PaymentService::NodeFactory::createNode(
      config.remoteNodeConfig.daemonHost, 
      config.remoteNodeConfig.daemonPort,
      log.getLogger()));

  runWalletService(currency, *node);
}

void PaymentGateService::runWalletService(const CryptoNote::Currency& currency, CryptoNote::INode& node) {
  PaymentService::WalletConfiguration walletConfiguration{
    config.gateConfiguration.containerFile,
    config.gateConfiguration.containerPassword,
    config.gateConfiguration.syncFromZero
  };

  std::unique_ptr<CryptoNote::WalletGreen> wallet(new CryptoNote::WalletGreen(*dispatcher, currency, node, logger));

  service = new PaymentService::WalletService(currency, *dispatcher, node, *wallet, *wallet, walletConfiguration, logger);
  std::unique_ptr<PaymentService::WalletService> serviceGuard(service);
  try {
    service->init();
  } catch (std::exception& e) {
    Logging::LoggerRef(logger, "run")(Logging::ERROR, Logging::BRIGHT_RED) << "Failed to init walletService reason: " << e.what();
    return;
  }

  if (config.gateConfiguration.printAddresses) {
    // print addresses and exit
    std::vector<std::string> addresses;
    service->getAddresses(addresses);
    for (const auto& address: addresses) {
      std::cout << "Address: " << address << std::endl;
    }
  } else {
    PaymentService::PaymentServiceJsonRpcServer rpcServer(*dispatcher, *stopEvent, *service, logger);
    rpcServer.start(config.gateConfiguration.bindAddress, config.gateConfiguration.bindPort);

    Logging::LoggerRef(logger, "PaymentGateService")(Logging::INFO, Logging::BRIGHT_WHITE) << "JSON-RPC server stopped, stopping wallet service...";

    try {
      service->saveWallet();
    } catch (std::exception& ex) {
      Logging::LoggerRef(logger, "saveWallet")(Logging::WARNING, Logging::YELLOW) << "Couldn't save container: " << ex.what();
    }
  }
}
