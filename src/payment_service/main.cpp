// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include <iostream>
#include <memory>
#include <thread>

#include <string.h>

#include "PaymentServiceConfiguration.h"
#include "JsonRpcServer.h"
#include "WalletService.h"
#include "cryptonote_core/Currency.h"
#include "Common/SignalHandler.h"
#include "Logging/LoggerGroup.h"
#include "Logging/ConsoleLogger.h"
#include "Logging/LoggerRef.h"
#include "Logging/StreamLogger.h"
#include "NodeFactory.h"

#include <boost/asio/io_service.hpp>

#ifdef WIN32
#include <windows.h>
#include <winsvc.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#endif

#include "ConfigurationManager.h"
#include "cryptonote_core/CoreConfig.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "p2p/net_node.h"
#include "InProcessNode/InProcessNode.h"
#include "version.h"

#define SERVICE_NAME "Payment Gate"

PaymentService::ConfigurationManager config;
System::Dispatcher systemService;
System::Event stopEvent(systemService);
PaymentService::WalletService* service;
std::unique_ptr<CryptoNote::CurrencyBuilder> currencyBuilder;
Logging::LoggerGroup logger;
CryptoNote::node_server * gP2pNode = nullptr;

#ifdef WIN32
SERVICE_STATUS_HANDLE serviceStatusHandle;
#endif

void run();

void stopSignalHandler() {
  Logging::LoggerRef log(logger, "StopSignalHandler");
  log(Logging::INFO) << "Stop signal caught";

  try {
    if (service) {
      service->saveWallet();
    }
  } catch (std::exception& ex) {
    log(Logging::WARNING) << "Couldn't save wallet: " << ex.what();
  }


  if (gP2pNode != nullptr) {
    gP2pNode->send_stop_signal();
  } else {
    stopEvent.set();
  }
}

#ifdef WIN32
std::string GetLastErrorMessage(DWORD errorMessageID)
{
  LPSTR messageBuffer = nullptr;
  size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, errorMessageID, 0, (LPSTR)&messageBuffer, 0, NULL);

  std::string message(messageBuffer, size);

  LocalFree(messageBuffer);

  return message;
}

void __stdcall serviceHandler(DWORD fdwControl) {
  if (fdwControl == SERVICE_CONTROL_STOP) {
    Logging::LoggerRef log(logger, "serviceHandler");
    log(Logging::INFO) << "Stop signal caught";

    SERVICE_STATUS serviceStatus{ SERVICE_WIN32_OWN_PROCESS, SERVICE_STOP_PENDING, 0, NO_ERROR, 0, 0, 0 };
    SetServiceStatus(serviceStatusHandle, &serviceStatus);

    try {
      if (service) {
        log(Logging::INFO) << "Saving wallet";
        service->saveWallet();
      }
    } catch (std::exception& ex) {
      log(Logging::WARNING) << "Couldn't save wallet: " << ex.what();
    }

    log(Logging::INFO) << "Stopping service";
    stopEvent.set();
  }
}

void __stdcall serviceMain(DWORD dwArgc, char **lpszArgv) {
  Logging::LoggerRef logRef(logger, "WindowsService");

  serviceStatusHandle = RegisterServiceCtrlHandler("PaymentGate", serviceHandler);
  if (serviceStatusHandle == NULL) {
    logRef(Logging::FATAL) << "Couldn't make RegisterServiceCtrlHandler call: " << GetLastErrorMessage(GetLastError());
    return;
  }

  SERVICE_STATUS serviceStatus{ SERVICE_WIN32_OWN_PROCESS, SERVICE_START_PENDING, 0, NO_ERROR, 0, 1, 3000 };
  if (SetServiceStatus(serviceStatusHandle, &serviceStatus) != TRUE) {
    logRef(Logging::FATAL) << "Couldn't make SetServiceStatus call: " << GetLastErrorMessage(GetLastError());
    return;
  }

  serviceStatus = { SERVICE_WIN32_OWN_PROCESS, SERVICE_RUNNING, SERVICE_ACCEPT_STOP, NO_ERROR, 0, 0, 0 };
  if (SetServiceStatus(serviceStatusHandle, &serviceStatus) != TRUE) {
    logRef(Logging::FATAL) << "Couldn't make SetServiceStatus call: " << GetLastErrorMessage(GetLastError());
    return;
  }

  try {
    run();
  } catch (std::exception& ex) {
    logRef(Logging::FATAL) << "Error occured: " << ex.what();
  }

  serviceStatus = { SERVICE_WIN32_OWN_PROCESS, SERVICE_STOPPED, 0, NO_ERROR, 0, 0, 0 };
  SetServiceStatus(serviceStatusHandle, &serviceStatus);
}
#else
int daemonize() {
  pid_t pid;
  pid = fork();

  if (pid < 0)
    return pid;

  if (pid > 0)
    return pid;

  if (setsid() < 0)
    return -1;

  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);

  pid = fork();

  if (pid < 0)
    return pid;

  if (pid > 0)
    return pid;

  umask(0);

  return 0;
}
#endif

int runDaemon() {
#ifdef WIN32

  SERVICE_TABLE_ENTRY serviceTable[] {
    { "PaymentGate", serviceMain },
    { NULL, NULL }
  };

  if (StartServiceCtrlDispatcher(serviceTable) != TRUE) {
    return 1;
  }

  return 0;

#else

  int daemonResult = daemonize();
  if (daemonResult > 0) {
    //parent
    return 0;
  } else if (daemonResult < 0) {
    //error occured
    return 1;
  }

  run();
  return 0;

#endif
}

int registerService() {
#ifdef WIN32
  Logging::LoggerRef logRef(logger, "ServiceRegistrator");

  char pathBuff[MAX_PATH];
  std::string modulePath;
  SC_HANDLE scManager = NULL;
  SC_HANDLE scService = NULL;
  int ret = 0;

  for (;;) {
    if (GetModuleFileName(NULL, pathBuff, ARRAYSIZE(pathBuff)) == 0) {
      logRef(Logging::FATAL) << "GetModuleFileName failed with error: " << GetLastErrorMessage(GetLastError());
      ret = 1;
      break;
    }

    modulePath.assign(pathBuff);

    std::string moduleDir = modulePath.substr(0, modulePath.find_last_of('\\') + 1);
    modulePath += " --config=" + moduleDir + "payment_service.conf -d";

    scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
    if (scManager == NULL) {
      logRef(Logging::FATAL) << "OpenSCManager failed with error: " << GetLastErrorMessage(GetLastError());
      ret = 1;
      break;
    }

    scService = CreateService(scManager, SERVICE_NAME, NULL, SERVICE_QUERY_STATUS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
      SERVICE_ERROR_NORMAL, modulePath.c_str(), NULL, NULL, NULL, NULL, NULL);

    if (scService == NULL) {
      logRef(Logging::FATAL) << "CreateService failed with error: " << GetLastErrorMessage(GetLastError());
      ret = 1;
      break;
    }

    logRef(Logging::INFO) << "Service is registered successfully";
    logRef(Logging::INFO) << "Please make sure " << moduleDir + "payment_service.conf" << " exists";
    break;
  }

  if (scManager) {
    CloseServiceHandle(scManager);
  }

  if (scService) {
    CloseServiceHandle(scService);
  }

  return ret;
#else
  return 0;
#endif
}

int unregisterService() {
#ifdef WIN32
  Logging::LoggerRef logRef(logger, "ServiceDeregistrator");

  SC_HANDLE scManager = NULL;
  SC_HANDLE scService = NULL;
  SERVICE_STATUS ssSvcStatus = { };
  int ret = 0;

  for (;;) {
    scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (scManager == NULL) {
      logRef(Logging::FATAL) << "OpenSCManager failed with error: " << GetLastErrorMessage(GetLastError());
      ret = 1;
      break;
    }

    scService = OpenService(scManager, SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
    if (scService == NULL) {
      logRef(Logging::FATAL) << "OpenService failed with error: " << GetLastErrorMessage(GetLastError());
      ret = 1;
      break;
    }

    if (ControlService(scService, SERVICE_CONTROL_STOP, &ssSvcStatus)) {
      logRef(Logging::INFO) << "Stopping " << SERVICE_NAME;
      Sleep(1000);

      while (QueryServiceStatus(scService, &ssSvcStatus)) {
        if (ssSvcStatus.dwCurrentState == SERVICE_STOP_PENDING) {
          logRef(Logging::INFO) << "Waiting...";
          Sleep(1000);
        } else {
          break;
        }
      }

      std::cout << std::endl;
      if (ssSvcStatus.dwCurrentState == SERVICE_STOPPED) {
        logRef(Logging::INFO) << SERVICE_NAME << " is stopped";
      } else {
        logRef(Logging::FATAL) << SERVICE_NAME << " failed to stop" << std::endl;
      }
    }

    if (!DeleteService(scService)) {
      logRef(Logging::FATAL) << "DeleteService failed with error: " << GetLastErrorMessage(GetLastError());
      ret = 1;
      break;
    }

    logRef(Logging::INFO) << SERVICE_NAME << " is removed";
    break;
  }

  if (scManager) {
    CloseServiceHandle(scManager);
  }

  if (scService) {
    CloseServiceHandle(scService);
  }

  return ret;
#else
  return 0;
#endif
}

void changeDirectory(const std::string& path) {
#ifndef WIN32
  //unix
  if (chdir(path.c_str())) {
    throw std::runtime_error("Couldn't change directory to \'" + path + "\': " + strerror(errno));
  }
#else
  if (!SetCurrentDirectory(path.c_str())) {
    throw std::runtime_error("Couldn't change directory to \'" + path + "\': " + GetLastErrorMessage(GetLastError()));
  }
#endif
}

void runInProcess() {
  Logging::LoggerRef(logger, "run")(Logging::INFO) << "Starting Payment Gate with local node";

currencyBuilder->genesisCoinbaseTxHex(config.coinBaseConfig.GENESIS_COINBASE_TX_HEX);
currencyBuilder->publicAddressBase58Prefix(config.coinBaseConfig.CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);
currencyBuilder->moneySupply(config.coinBaseConfig.MONEY_SUPPLY);
currencyBuilder->emissionSpeedFactor(config.coinBaseConfig.EMISSION_SPEED_FACTOR);
currencyBuilder->blockGrantedFullRewardZone(config.coinBaseConfig.CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
currencyBuilder->numberOfDecimalPlaces(config.coinBaseConfig.CRYPTONOTE_DISPLAY_DECIMAL_POINT);
currencyBuilder->mininumFee(config.coinBaseConfig.MINIMUM_FEE);
currencyBuilder->defaultDustThreshold(config.coinBaseConfig.DEFAULT_DUST_THRESHOLD);
currencyBuilder->difficultyTarget(config.coinBaseConfig.DIFFICULTY_TARGET);
currencyBuilder->minedMoneyUnlockWindow(config.coinBaseConfig.CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);
currencyBuilder->maxBlockSizeInitial(config.coinBaseConfig.MAX_BLOCK_SIZE_INITIAL);

if (config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY && config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY != 0)
{
  currencyBuilder->difficultyWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
  currencyBuilder->upgradeVotingWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
  currencyBuilder->upgradeWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
} else {
  currencyBuilder->difficultyWindow(24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
}
currencyBuilder->maxBlockSizeGrowthSpeedDenominator(365 * 24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
currencyBuilder->lockedTxAllowedDeltaSeconds(config.coinBaseConfig.DIFFICULTY_TARGET * CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);

if (config.coinBaseConfig.UPGRADE_HEIGHT && config.coinBaseConfig.UPGRADE_HEIGHT != 0)
{
  currencyBuilder->upgradeHeight(config.coinBaseConfig.UPGRADE_HEIGHT);
}
  CryptoNote::Currency currency = currencyBuilder->currency();
  CryptoNote::core core(currency, NULL, logger);

  CryptoNote::cryptonote_protocol_handler protocol(currency, systemService, core, NULL, logger);
  CryptoNote::node_server p2pNode(systemService, protocol, logger);
  gP2pNode = &p2pNode;

  protocol.set_p2p_endpoint(&p2pNode);
  core.set_cryptonote_protocol(&protocol);

  std::unique_ptr<CryptoNote::INode> node;

  Logging::LoggerRef(logger, "run")(Logging::INFO) << "initializing p2pNode";
  if (!p2pNode.init(config.netNodeConfig, config.gateConfiguration.testnet)) {
    throw std::runtime_error("Failed to init p2pNode");
  }

  Logging::LoggerRef(logger, "run")(Logging::INFO) << "initializing core";
  CryptoNote::MinerConfig emptyMiner;
  core.init(config.coreConfig, emptyMiner, true);

  std::promise<std::error_code> initPromise;
  auto initFuture = initPromise.get_future();

  node.reset(new CryptoNote::InProcessNode(core, protocol));
  node->init([&initPromise](std::error_code ec) {
    if (ec) {
      Logging::LoggerRef(logger, "run")(Logging::INFO) << "Failed to init node: " << ec.message();
    } else {
      Logging::LoggerRef(logger, "run")(Logging::INFO) << "node is inited successfully";
    }

    initPromise.set_value(ec);
  });

  auto ec = initFuture.get();
  if (ec) {
    throw std::system_error(ec);
  }

  Logging::LoggerRef(logger, "run")(Logging::INFO) << "Starting p2p server";

  System::Event p2pStarted(systemService);

  systemService.spawn([&]() {
    p2pStarted.set();
    p2pNode.run();
    stopEvent.set();
  });

  p2pStarted.wait();
  Logging::LoggerRef(logger, "run")(Logging::INFO) << "p2p server is started";

  service = new PaymentService::WalletService(currency, systemService, *node, config.gateConfiguration, logger);
  std::unique_ptr<PaymentService::WalletService> serviceGuard(service);

  service->init();

  PaymentService::JsonRpcServer rpcServer(systemService, stopEvent, *service, logger);

  rpcServer.start(config.gateConfiguration);

  serviceGuard.reset();
  node->shutdown();
  core.deinit();
  p2pNode.deinit();
  gP2pNode = nullptr;
}

void runRpcProxy() {
  Logging::LoggerRef(logger, "run")(Logging::INFO) << "Starting Payment Gate with remote node";
currencyBuilder->genesisCoinbaseTxHex(config.coinBaseConfig.GENESIS_COINBASE_TX_HEX);
currencyBuilder->publicAddressBase58Prefix(config.coinBaseConfig.CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);
currencyBuilder->moneySupply(config.coinBaseConfig.MONEY_SUPPLY);
currencyBuilder->emissionSpeedFactor(config.coinBaseConfig.EMISSION_SPEED_FACTOR);
currencyBuilder->blockGrantedFullRewardZone(config.coinBaseConfig.CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
currencyBuilder->numberOfDecimalPlaces(config.coinBaseConfig.CRYPTONOTE_DISPLAY_DECIMAL_POINT);
currencyBuilder->mininumFee(config.coinBaseConfig.MINIMUM_FEE);
currencyBuilder->defaultDustThreshold(config.coinBaseConfig.DEFAULT_DUST_THRESHOLD);
currencyBuilder->difficultyTarget(config.coinBaseConfig.DIFFICULTY_TARGET);
currencyBuilder->minedMoneyUnlockWindow(config.coinBaseConfig.CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);
currencyBuilder->maxBlockSizeInitial(config.coinBaseConfig.MAX_BLOCK_SIZE_INITIAL);

if (config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY && config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY != 0)
{
  currencyBuilder->difficultyWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
  currencyBuilder->upgradeVotingWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
  currencyBuilder->upgradeWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
} else {
  currencyBuilder->difficultyWindow(24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
}
currencyBuilder->maxBlockSizeGrowthSpeedDenominator(365 * 24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
currencyBuilder->lockedTxAllowedDeltaSeconds(config.coinBaseConfig.DIFFICULTY_TARGET * CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);

if (config.coinBaseConfig.UPGRADE_HEIGHT && config.coinBaseConfig.UPGRADE_HEIGHT != 0)
{
  currencyBuilder->upgradeHeight(config.coinBaseConfig.UPGRADE_HEIGHT);
}
  CryptoNote::Currency currency = currencyBuilder->currency();
  std::unique_ptr<CryptoNote::INode> node;

  node.reset(PaymentService::NodeFactory::createNode(config.remoteNodeConfig.daemonHost, config.remoteNodeConfig.daemonPort));

  service = new PaymentService::WalletService(currency, systemService, *node, config.gateConfiguration, logger);
  std::unique_ptr<PaymentService::WalletService> serviceGuard(service);

  service->init();

  PaymentService::JsonRpcServer rpcServer(systemService, stopEvent, *service, logger);

  rpcServer.start(config.gateConfiguration);
}

void run() {
  tools::SignalHandler::install(stopSignalHandler);

  if (config.startInprocess) {
    runInProcess();
  } else {
    runRpcProxy();
  }
}

int main(int argc, char** argv) {
  try {
    if (!config.init(argc, argv)) {
      return 0; //help message requested or so
    }

    Logging::ConsoleLogger consoleLogger(static_cast<Logging::Level>(config.gateConfiguration.logLevel));
    logger.addLogger(consoleLogger);

    currencyBuilder.reset(new CryptoNote::CurrencyBuilder(logger));

    Logging::LoggerRef(logger, "main")(Logging::INFO) << "PaymentService " << " v" << PROJECT_VERSION_LONG;

    if (config.gateConfiguration.testnet) {
      Logging::LoggerRef(logger, "main")(Logging::INFO) << "Starting in testnet mode";
      currencyBuilder->testnet(true);
    }

    if (!config.gateConfiguration.serverRoot.empty()) {
      changeDirectory(config.gateConfiguration.serverRoot);
      Logging::LoggerRef(logger, "main")(Logging::INFO) << "Current working directory now is " << config.gateConfiguration.serverRoot;
    }

    std::ofstream fileStream(config.gateConfiguration.logFile, std::ofstream::app);
    if (!fileStream) {
      throw std::runtime_error("Couldn't open log file");
    }

    Logging::StreamLogger fileLogger(fileStream, static_cast<Logging::Level>(config.gateConfiguration.logLevel));
    logger.addLogger(fileLogger);

    if (config.gateConfiguration.generateNewWallet) {
currencyBuilder->genesisCoinbaseTxHex(config.coinBaseConfig.GENESIS_COINBASE_TX_HEX);
currencyBuilder->publicAddressBase58Prefix(config.coinBaseConfig.CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);
currencyBuilder->moneySupply(config.coinBaseConfig.MONEY_SUPPLY);
currencyBuilder->emissionSpeedFactor(config.coinBaseConfig.EMISSION_SPEED_FACTOR);
currencyBuilder->blockGrantedFullRewardZone(config.coinBaseConfig.CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
currencyBuilder->numberOfDecimalPlaces(config.coinBaseConfig.CRYPTONOTE_DISPLAY_DECIMAL_POINT);
currencyBuilder->mininumFee(config.coinBaseConfig.MINIMUM_FEE);
currencyBuilder->defaultDustThreshold(config.coinBaseConfig.DEFAULT_DUST_THRESHOLD);
currencyBuilder->difficultyTarget(config.coinBaseConfig.DIFFICULTY_TARGET);
currencyBuilder->minedMoneyUnlockWindow(config.coinBaseConfig.CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);
currencyBuilder->maxBlockSizeInitial(config.coinBaseConfig.MAX_BLOCK_SIZE_INITIAL);

if (config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY && config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY != 0)
{
  currencyBuilder->difficultyWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
  currencyBuilder->upgradeVotingWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
  currencyBuilder->upgradeWindow(config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
} else {
  currencyBuilder->difficultyWindow(24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
}
currencyBuilder->maxBlockSizeGrowthSpeedDenominator(365 * 24 * 60 * 60 / config.coinBaseConfig.DIFFICULTY_TARGET);
currencyBuilder->lockedTxAllowedDeltaSeconds(config.coinBaseConfig.DIFFICULTY_TARGET * CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);

if (config.coinBaseConfig.UPGRADE_HEIGHT && config.coinBaseConfig.UPGRADE_HEIGHT != 0)
{
  currencyBuilder->upgradeHeight(config.coinBaseConfig.UPGRADE_HEIGHT);
}
      CryptoNote::Currency currency = currencyBuilder->currency();
      generateNewWallet(currency, config.gateConfiguration, logger);
      return 0;
    }

    if (!config.gateConfiguration.importKeys.empty()) {
      importLegacyKeys(config.gateConfiguration);
      Logging::LoggerRef(logger, "KeysImporter")(Logging::INFO) << "Keys have been imported successfully";
      return 0;
    }

    if (config.gateConfiguration.registerService) {
      return registerService();
    }

    if (config.gateConfiguration.unregisterService) {
      return unregisterService();
    }

    if (config.gateConfiguration.daemonize) {
      logger.removeLogger(consoleLogger);
      if (runDaemon() != 0) {
        throw std::runtime_error("Failed to start daemon");
      }
    } else {
      run();
    }

  } catch (PaymentService::ConfigurationError& ex) {
    std::cerr << "Configuration error: " << ex.what() << std::endl;
    return 1;
  } catch (std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
