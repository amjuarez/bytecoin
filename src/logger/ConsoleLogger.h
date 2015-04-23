// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <mutex>
#include "CommonLogger.h"

namespace Log {

class ConsoleLogger : public CommonLogger {
public:
  ConsoleLogger(ILogger::Level level = ILogger::DEBUGGING);

protected:
  virtual void doLogString(Level level, boost::posix_time::ptime time, const std::string& message) override;

private:
  std::mutex mutex;
};

}
