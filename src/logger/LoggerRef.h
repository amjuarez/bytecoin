// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ILogger.h"
#include "LoggerMessage.h"

namespace Log {

class LoggerRef {
public:
  LoggerRef(const LoggerRef& other);
  LoggerRef(const LoggerRef& other, const std::string& category);
  LoggerRef(ILogger& logger, const std::string& category);
  LoggerMessage operator()(const std::string& category, ILogger::Level level, const std::string& color = ILogger::DEFAULT);
  LoggerMessage operator()(ILogger::Level level = ILogger::INFO, const std::string& color = ILogger::DEFAULT);

private:
  ILogger& logger;
  std::string category;
};

}
