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
#ifndef NOMINMAX
#define NOMINMAX
#endif
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

struct PaymentGate {
  PaymentGate() : dispatcher(nullptr), stopEvent(nullptr), config(), service(nullptr), logger(), currencyBuilder(logger) {
  }

  System::Dispatcher* dispatcher;
  System::Event* stopEvent;
  PaymentService::ConfigurationManager config;
  PaymentService::WalletService* service;
  Logging::LoggerGroup logger;
  CryptoNote::CurrencyBuilder currencyBuilder;
};

PaymentGate* ppg;

#ifdef WIN32
SERVICE_STATUS_HANDLE serviceStatusHandle;
#endif

void run();

void stopSignalHandler() {
  Logging::LoggerRef log(ppg->logger, "StopSignalHandler");
  log(Logging::INFO) << "Stop signal caught";

  try {
    if (ppg->service) {
      ppg->service->saveWallet();
    }
  } catch (std::exception& ex) {
    log(Logging::WARNING) << "Couldn't save wallet: " << ex.what();
  }

  ppg->dispatcher->remoteSpawn([&]() {
    ppg->stopEvent->set();
  });
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
    Logging::LoggerRef log(ppg->logger, "serviceHandler");
    log(Logging::INFO) << "Stop signal caught";

    SERVICE_STATUS serviceStatus{ SERVICE_WIN32_OWN_PROCESS, SERVICE_STOP_PENDING, 0, NO_ERROR, 0, 0, 0 };
    SetServiceStatus(serviceStatusHandle, &serviceStatus);

    try {
      if (ppg->service) {
        log(Logging::INFO) << "Saving wallet";
        ppg->service->saveWallet();
      }
    } catch (std::exception& ex) {
      log(Logging::WARNING) << "Couldn't save wallet: " << ex.what();
    }

    log(Logging::INFO) << "Stopping service";
    ppg->dispatcher->remoteSpawn([&]() {
      ppg->stopEvent->set();
    });
  }
}

void __stdcall serviceMain(DWORD dwArgc, char **lpszArgv) {
  System::Dispatcher dispatcher;
  System::Event stopEvent(dispatcher);
  ppg->dispatcher = &dispatcher;
  ppg->stopEvent = &stopEvent;
  Logging::LoggerRef logRef(ppg->logger, "WindowsService");

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
    { "Payment Gate", serviceMain },
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

  System::Dispatcher dispatcher;
  System::Event stopEvent(dispatcher);
  ppg->dispatcher = &dispatcher;
  ppg->stopEvent = &stopEvent;
  run();
  return 0;

#endif
}

int registerService() {
#ifdef WIN32
  Logging::LoggerRef logRef(ppg->logger, "ServiceRegistrator");

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
  Logging::LoggerRef logRef(ppg->logger, "ServiceDeregistrator");

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
  Logging::LoggerRef(ppg->logger, "run")(Logging::INFO) << "Starting Payment Gate with local node";

  CryptoNote::Currency currency = ppg->currencyBuilder.currency();
  CryptoNote::core core(currency, NULL, ppg->logger);

  CryptoNote::cryptonote_protocol_handler protocol(currency, *ppg->dispatcher, core, NULL, ppg->logger);
  CryptoNote::node_server p2pNode(*ppg->dispatcher, protocol, ppg->logger);

  protocol.set_p2p_endpoint(&p2pNode);
  core.set_cryptonote_protocol(&protocol);

  std::unique_ptr<CryptoNote::INode> node;

  Logging::LoggerRef(ppg->logger, "run")(Logging::INFO) << "initializing p2pNode";
  if (!p2pNode.init(ppg->config.netNodeConfig, ppg->config.gateConfiguration.testnet)) {
    throw std::runtime_error("Failed to init p2pNode");
  }

  Logging::LoggerRef(ppg->logger, "run")(Logging::INFO) << "initializing core";
  CryptoNote::MinerConfig emptyMiner;
  core.init(ppg->config.coreConfig, emptyMiner, true);

  std::promise<std::error_code> initPromise;
  auto initFuture = initPromise.get_future();

  node.reset(new CryptoNote::InProcessNode(core, protocol));
  node->init([&initPromise](std::error_code ec) {
    if (ec) {
      Logging::LoggerRef(ppg->logger, "run")(Logging::INFO) << "Failed to init node: " << ec.message();
    } else {
      Logging::LoggerRef(ppg->logger, "run")(Logging::INFO) << "node is inited successfully";
    }

    initPromise.set_value(ec);
  });

  auto ec = initFuture.get();
  if (ec) {
    throw std::system_error(ec);
  }

  Logging::LoggerRef(ppg->logger, "run")(Logging::INFO) << "Spawning p2p server";

  System::Event p2pStopped(*ppg->dispatcher);
  System::Event p2pStarted(*ppg->dispatcher);

  ppg->dispatcher->spawn([&]() {
    p2pStarted.set();
    p2pNode.run();
    p2pStopped.set();
  });
  
  p2pStarted.wait();
  Logging::LoggerRef(ppg->logger, "run")(Logging::INFO) << "p2p server is started";

  ppg->service = new PaymentService::WalletService(currency, *ppg->dispatcher, *node, ppg->config.gateConfiguration, ppg->logger);
  std::unique_ptr<PaymentService::WalletService> serviceGuard(ppg->service);

  ppg->service->init();

  PaymentService::JsonRpcServer rpcServer(*ppg->dispatcher, *ppg->stopEvent, *ppg->service, ppg->logger);

  rpcServer.start(ppg->config.gateConfiguration);
  serviceGuard.reset();
  p2pNode.send_stop_signal();
  p2pStopped.wait();
  node->shutdown();
  core.deinit();
  p2pNode.deinit();
}

void runRpcProxy() {
  Logging::LoggerRef(ppg->logger, "run")(Logging::INFO) << "Starting Payment Gate with remote node";
  CryptoNote::Currency currency = ppg->currencyBuilder.currency();
  std::unique_ptr<CryptoNote::INode> node;

  node.reset(PaymentService::NodeFactory::createNode(ppg->config.remoteNodeConfig.daemonHost, ppg->config.remoteNodeConfig.daemonPort));

  ppg->service = new PaymentService::WalletService(currency, *ppg->dispatcher, *node, ppg->config.gateConfiguration, ppg->logger);
  std::unique_ptr<PaymentService::WalletService> serviceGuard(ppg->service);

  ppg->service->init();

  PaymentService::JsonRpcServer rpcServer(*ppg->dispatcher, *ppg->stopEvent, *ppg->service, ppg->logger);

  rpcServer.start(ppg->config.gateConfiguration);
}

void run() {
  tools::SignalHandler::install(stopSignalHandler);

  if (ppg->config.startInprocess) {
    runInProcess();
  } else {
    runRpcProxy();
  }
}

int main(int argc, char** argv) {
  PaymentGate pg; 
  ppg = &pg;
  try {
    if (!pg.config.init(argc, argv)) {
      return 0; //help message requested or so
    }

    Logging::ConsoleLogger consoleLogger(static_cast<Logging::Level>(pg.config.gateConfiguration.logLevel));
    pg.logger.addLogger(consoleLogger);

    Logging::LoggerRef(pg.logger, "main")(Logging::INFO) << "PaymentService " << " v" << PROJECT_VERSION_LONG;

    if (pg.config.gateConfiguration.testnet) {
      Logging::LoggerRef(pg.logger, "main")(Logging::INFO) << "Starting in testnet mode";
      pg.currencyBuilder.testnet(true);
    }

    if (!pg.config.gateConfiguration.serverRoot.empty()) {
      changeDirectory(pg.config.gateConfiguration.serverRoot);
      Logging::LoggerRef(pg.logger, "main")(Logging::INFO) << "Current working directory now is " << pg.config.gateConfiguration.serverRoot;
    }

    std::ofstream fileStream(pg.config.gateConfiguration.logFile, std::ofstream::app);
    if (!fileStream) {
      throw std::runtime_error("Couldn't open log file");
    }

    Logging::StreamLogger fileLogger(fileStream, static_cast<Logging::Level>(pg.config.gateConfiguration.logLevel));
    pg.logger.addLogger(fileLogger);

  pg.currencyBuilder.genesisCoinbaseTxHex(pg.config.coinBaseConfig.GENESIS_COINBASE_TX_HEX);
  pg.currencyBuilder.publicAddressBase58Prefix(pg.config.coinBaseConfig.CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);
  pg.currencyBuilder.moneySupply(pg.config.coinBaseConfig.MONEY_SUPPLY);
  pg.currencyBuilder.emissionSpeedFactor(pg.config.coinBaseConfig.EMISSION_SPEED_FACTOR);
  pg.currencyBuilder.blockGrantedFullRewardZone(pg.config.coinBaseConfig.CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
  pg.currencyBuilder.numberOfDecimalPlaces(pg.config.coinBaseConfig.CRYPTONOTE_DISPLAY_DECIMAL_POINT);
  pg.currencyBuilder.mininumFee(pg.config.coinBaseConfig.MINIMUM_FEE);
  pg.currencyBuilder.defaultDustThreshold(pg.config.coinBaseConfig.DEFAULT_DUST_THRESHOLD);
  pg.currencyBuilder.difficultyTarget(pg.config.coinBaseConfig.DIFFICULTY_TARGET);
  pg.currencyBuilder.minedMoneyUnlockWindow(pg.config.coinBaseConfig.CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);
  pg.currencyBuilder.maxBlockSizeInitial(pg.config.coinBaseConfig.MAX_BLOCK_SIZE_INITIAL);
  if (pg.config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY && pg.config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY != 0)
  {
    pg.currencyBuilder.difficultyWindow(pg.config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
    pg.currencyBuilder.upgradeVotingWindow(pg.config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
    pg.currencyBuilder.upgradeWindow(pg.config.coinBaseConfig.EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
  } else {
    pg.currencyBuilder.difficultyWindow(24 * 60 * 60 / pg.config.coinBaseConfig.DIFFICULTY_TARGET);
  }
  pg.currencyBuilder.maxBlockSizeGrowthSpeedDenominator(365 * 24 * 60 * 60 / pg.config.coinBaseConfig.DIFFICULTY_TARGET);
  pg.currencyBuilder.lockedTxAllowedDeltaSeconds(pg.config.coinBaseConfig.DIFFICULTY_TARGET * CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);
  if (pg.config.coinBaseConfig.UPGRADE_HEIGHT && pg.config.coinBaseConfig.UPGRADE_HEIGHT != 0)
  {
    pg.currencyBuilder.upgradeHeight(pg.config.coinBaseConfig.UPGRADE_HEIGHT);
  }
  pg.currencyBuilder.difficultyLag(pg.config.coinBaseConfig.DIFFICULTY_CUT);
  pg.currencyBuilder.difficultyCut(pg.config.coinBaseConfig.DIFFICULTY_LAG);
    if (pg.config.gateConfiguration.generateNewWallet) {
      CryptoNote::Currency currency = pg.currencyBuilder.currency();
      generateNewWallet(currency, pg.config.gateConfiguration, pg.logger);
      return 0;
    }

    if (!pg.config.gateConfiguration.importKeys.empty()) {
      importLegacyKeys(pg.config.gateConfiguration);
      Logging::LoggerRef(pg.logger, "KeysImporter")(Logging::INFO) << "Keys have been imported successfully";
      return 0;
    }

    if (pg.config.gateConfiguration.registerService) {
      return registerService();
    }

    if (pg.config.gateConfiguration.unregisterService) {
      return unregisterService();
    }

    if (pg.config.gateConfiguration.daemonize) {
      pg.logger.removeLogger(consoleLogger);
      if (runDaemon() != 0) {
        throw std::runtime_error("Failed to start daemon");
      }
    } else {
      System::Dispatcher dispatcher;
      System::Event stopEvent(dispatcher);
      ppg->dispatcher = &dispatcher;
      ppg->stopEvent = &stopEvent;
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
