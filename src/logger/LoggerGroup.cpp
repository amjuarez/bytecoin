#include "LoggerGroup.h"
#include <algorithm>

using namespace Log;

LoggerGroup::LoggerGroup(ILogger::Level level) : CommonLogger(level) {
}

void LoggerGroup::addLogger(ILogger& logger) {
  loggers.push_back(&logger);
}

void LoggerGroup::removeLogger(ILogger& logger) {
  loggers.erase(std::remove(loggers.begin(), loggers.end(), &logger), loggers.end());
}

void LoggerGroup::operator()(const std::string& category, Level level, boost::posix_time::ptime time, const std::string& body) {
  if (level > logLevel) {
    return;
  }

  if (disabledCategories.count(category) != 0) {
    return;
  }

  for (auto& logger: loggers) { 
    (*logger)(category, level, time, body); 
  }
}
