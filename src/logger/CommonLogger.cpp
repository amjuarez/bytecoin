// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CommonLogger.h"

#include <sstream>

using namespace Log;

CommonLogger::CommonLogger(ILogger::Level level) : logLevel(level) {
}

void CommonLogger::doLogString(Level level, boost::posix_time::ptime time, const std::string& message) {
}

void CommonLogger::enableCategory(const std::string& category) {
  disabledCategories.erase(category);
}

void CommonLogger::disableCategory(const std::string& category) {
  disabledCategories.insert(category);
}

void CommonLogger::setMaxLevel(Level level) {
  logLevel = level;
}

void CommonLogger::operator()(const std::string& category, Level level, boost::posix_time::ptime time, const std::string& body) {
  if (level > logLevel) {
    return;
  }

  if (disabledCategories.count(category) != 0) {
    return;
  }

  doLogString(level, time, body);
}
