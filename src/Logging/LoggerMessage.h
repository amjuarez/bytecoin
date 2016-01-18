// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <iostream>
#include "ILogger.h"

namespace Logging {

class LoggerMessage : public std::ostream, std::streambuf {
public:
  LoggerMessage(ILogger& logger, const std::string& category, Level level, const std::string& color);
  ~LoggerMessage();
  LoggerMessage(const LoggerMessage&) = delete;
  LoggerMessage& operator=(const LoggerMessage&) = delete;
  LoggerMessage(LoggerMessage&& other);

private:
  int sync() override;
  int overflow(int c) override;

  std::string message;
  const std::string category;
  Level logLevel;
  ILogger& logger;
  boost::posix_time::ptime timestamp;
  bool gotText;
};

}
