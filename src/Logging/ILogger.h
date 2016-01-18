// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <array>
#include <boost/date_time/posix_time/posix_time.hpp>

#undef ERROR

namespace Logging {

enum Level {
  FATAL = 0,
  ERROR = 1,
  WARNING = 2,
  INFO = 3,
  DEBUGGING = 4,
  TRACE = 5
};

extern const std::string BLUE;
extern const std::string GREEN;
extern const std::string RED;
extern const std::string YELLOW;
extern const std::string WHITE;
extern const std::string CYAN;
extern const std::string MAGENTA;
extern const std::string BRIGHT_BLUE;
extern const std::string BRIGHT_GREEN;
extern const std::string BRIGHT_RED;
extern const std::string BRIGHT_YELLOW;
extern const std::string BRIGHT_WHITE;
extern const std::string BRIGHT_CYAN;
extern const std::string BRIGHT_MAGENTA;
extern const std::string DEFAULT;

class ILogger {
public:
  const static char COLOR_DELIMETER;

  const static std::array<std::string, 6> LEVEL_NAMES;

  virtual void operator()(const std::string& category, Level level, boost::posix_time::ptime time, const std::string& body) = 0;
};

#ifndef ENDL
#define ENDL std::endl
#endif

}
