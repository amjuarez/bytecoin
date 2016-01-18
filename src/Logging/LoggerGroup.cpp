// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "LoggerGroup.h"
#include <algorithm>

namespace Logging {

LoggerGroup::LoggerGroup(Level level) : CommonLogger(level) {
}

void LoggerGroup::addLogger(ILogger& logger) {
  loggers.push_back(&logger);
}

void LoggerGroup::removeLogger(ILogger& logger) {
  loggers.erase(std::remove(loggers.begin(), loggers.end(), &logger), loggers.end());
}

void LoggerGroup::operator()(const std::string& category, Level level, boost::posix_time::ptime time, const std::string& body) {
  if (level <= logLevel && disabledCategories.count(category) == 0) {
    for (auto& logger : loggers) {
      (*logger)(category, level, time, body);
    }
  }
}

}
