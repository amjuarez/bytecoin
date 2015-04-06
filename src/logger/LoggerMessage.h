#pragma once

#include <iostream>
#include "ILogger.h"

namespace Log {

class LoggerMessage : public std::ostream, std::streambuf {
public:
  LoggerMessage(ILogger& logger, const std::string& category, ILogger::Level level, const std::string& color);
  ~LoggerMessage();
  LoggerMessage(const LoggerMessage&) = delete;
  LoggerMessage& operator=(const LoggerMessage&) = delete;
  LoggerMessage(LoggerMessage&& other);

private:
  int sync() override;
  int overflow(int c) override;

  std::string message;
  const std::string category;
  ILogger::Level logLevel;
  ILogger& logger;
  boost::posix_time::ptime timestamp;
  bool gotText;
};

}
