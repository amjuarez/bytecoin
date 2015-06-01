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

#include "PaymentServiceConfiguration.h"

#include <iostream>
#include <algorithm>
#include <boost/program_options.hpp>

#include "Logging/ILogger.h"

namespace po = boost::program_options;

namespace PaymentService {

Configuration::Configuration() {
  generateNewWallet = false;
  daemonize = false;
  registerService = false;
  unregisterService = false;
  logFile = "payment_gate.log";
  testnet = false;
  logLevel = Logging::INFO;
}

void Configuration::initOptions(boost::program_options::options_description& desc) {
  desc.add_options()
      ("bind-address", po::value<std::string>()->default_value("0.0.0.0"), "payment service bind address")
      ("bind-port", po::value<uint16_t>()->default_value(8070), "payment service bind port")
      ("wallet-file,w", po::value<std::string>(), "wallet file")
      ("wallet-password,p", po::value<std::string>(), "wallet password")
      ("generate-wallet,g", "generate new wallet file and exit")
      ("daemon,d", "run as daemon in Unix or as service in Windows")
      ("register-service", "register service and exit (Windows only)")
      ("unregister-service", "unregister service and exit (Windows only)")
      ("import-keys,i", po::value<std::string>(), "import legacy keys file and exit")
      ("log-file,l", po::value<std::string>(), "log file")
      ("server-root", po::value<std::string>(), "server root. The service will use it as working directory. Don't set it if don't want to change it")
      ("log-level", po::value<std::size_t>(), "log level");
}

void Configuration::init(const boost::program_options::variables_map& options) {
  if (options.count("daemon")) {
    daemonize = true;
  }

  if (options.count("register-service")) {
    registerService = true;
  }

  if (options.count("unregister-service")) {
    unregisterService = true;
  }

  if (registerService && unregisterService) {
    throw ConfigurationError("It's impossible to use both \"register-service\" and \"unregister-service\" at the same time");
  }

  if (options.count("testnet")) {
    testnet = true;
  }

  if (options.count("log-file")) {
    logFile = options["log-file"].as<std::string>();
  }

  if (options.count("log-level")) {
    logLevel = options["log-level"].as<std::size_t>();
    if (logLevel > Logging::TRACE) {
      std::string error = "log-level option must be in " + std::to_string(Logging::FATAL) +  ".." + std::to_string(Logging::TRACE) + " interval";
      throw ConfigurationError(error.c_str());
    }
  }

  if (options.count("server-root")) {
    serverRoot = options["server-root"].as<std::string>();
  }

  if (options.count("bind-address")) {
    bindAddress = options["bind-address"].as<std::string>();
  }

  if (options.count("bind-port")) {
    bindPort = options["bind-port"].as<uint16_t>();
  }

  if (options.count("wallet-file")) {
    walletFile = options["wallet-file"].as<std::string>();
  }

  if (options.count("wallet-password")) {
    walletPassword = options["wallet-password"].as<std::string>();
  }

  if (options.count("generate-wallet")) {
    generateNewWallet = true;
  }

  if (options.count("import-keys")) {
    importKeys = options["import-keys"].as<std::string>();
  }

  if (!importKeys.empty() && generateNewWallet) {
    throw ConfigurationError("It's impossible to use both \"import\" and \"generate-wallet\" at the same time");
  }

  if (!registerService && !unregisterService) {
    if (walletFile.empty() || walletPassword.empty()) {
      throw ConfigurationError("Both wallet-file and wallet-password parameters are required");
    }
  }
}

} //namespace PaymentService
