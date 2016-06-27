// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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
#include "InProcessNode/InProcessNode.h"
#include "Logging/LoggerRef.h"
#include "PaymentGate/PaymentServiceJsonRpcServer.h"

#include "CryptoNoteCore/CoreConfig.h"
#include "CryptoNoteCore/Core.h"
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
  currencyBuilder.genesisBlockReward(config.coinBaseConfig.GENESIS_BLOCK_REWARD);
  currencyBuilder.cryptonoteCoinVersion(config.coinBaseConfig.CRYPTONOTE_COIN_VERSION);
  currencyBuilder.killHeight(config.coinBaseConfig.KILL_HEIGHT);
  currencyBuilder.mandatoryTransaction(config.coinBaseConfig.MANDATORY_TRANSACTION);
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
    currencyBuilder.difficultyWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
    currencyBuilder.upgradeVotingWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
    currencyBuilder.upgradeWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
  } else {
    currencyBuilder.difficultyWindow(24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
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
  currencyBuilder.difficultyLag(config.coinBaseConfig.DIFFICULTY_LAG);
currencyBuilder.maxTransactionSizeLimit(config.coinBaseConfig.MAX_TRANSACTION_SIZE_LIMIT);
currencyBuilder.fusionTxMaxSize(config.coinBaseConfig.MAX_TRANSACTION_SIZE_LIMIT * 30 / 100);
  currencyBuilder.difficultyCut(config.coinBaseConfig.DIFFICULTY_CUT);
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
  if (!config.coreConfig.configFolderDefaulted) {
    if (!Tools::directoryExists(config.coreConfig.configFolder)) {
      throw std::runtime_error("Directory does not exist: " + config.coreConfig.configFolder);
    }
  } else {
    if (!Tools::create_directories_if_necessary(config.coreConfig.configFolder)) {
      throw std::runtime_error("Can't create directory: " + config.coreConfig.configFolder);
    }
  }

  log(Logging::INFO) << "Starting Payment Gate with local node";

  CryptoNote::Currency currency = currencyBuilder.currency();
  CryptoNote::core core(currency, NULL, logger, false);

  CryptoNote::CryptoNoteProtocolHandler protocol(currency, *dispatcher, core, NULL, logger);
  CryptoNote::NodeServer p2pNode(*dispatcher, protocol, logger);

  protocol.set_p2p_endpoint(&p2pNode);
  core.set_cryptonote_protocol(&protocol);

  log(Logging::INFO) << "initializing p2pNode";
  if (!p2pNode.init(config.netNodeConfig)) {
    throw std::runtime_error("Failed to init p2pNode");
  }

  log(Logging::INFO) << "initializing core";
  CryptoNote::MinerConfig emptyMiner;
  core.init(config.coreConfig, emptyMiner, true);

  std::promise<std::error_code> initPromise;
  auto initFuture = initPromise.get_future();

  std::unique_ptr<CryptoNote::INode> node(new CryptoNote::InProcessNode(core, protocol));

  node->init([&initPromise, &log](std::error_code ec) {
    if (ec) {
      log(Logging::WARNING, Logging::YELLOW) << "Failed to init node: " << ec.message();
    } else {
      log(Logging::INFO) << "node is inited successfully";
    }

    initPromise.set_value(ec);
  });

  auto ec = initFuture.get();
  if (ec) {
    throw std::system_error(ec);
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
  core.deinit();
  p2pNode.deinit(); 
}

void PaymentGateService::runRpcProxy(Logging::LoggerRef& log) {
  log(Logging::INFO) << "Starting Payment Gate with remote node";
  CryptoNote::Currency currency = currencyBuilder.currency();
  
  std::unique_ptr<CryptoNote::INode> node(
    PaymentService::NodeFactory::createNode(
      config.remoteNodeConfig.daemonHost, 
      config.remoteNodeConfig.daemonPort));

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
