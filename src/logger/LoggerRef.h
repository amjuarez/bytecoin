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
