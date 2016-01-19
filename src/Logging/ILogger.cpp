// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ILogger.h"

namespace Logging {

const std::string BLUE = "\x1F""BLUE\x1F";
const std::string GREEN = "\x1F""GREEN\x1F";
const std::string RED = "\x1F""RED\x1F";
const std::string YELLOW = "\x1F""YELLOW\x1F";
const std::string WHITE = "\x1F""WHITE\x1F";
const std::string CYAN = "\x1F""CYAN\x1F";
const std::string MAGENTA = "\x1F""MAGENTA\x1F";
const std::string BRIGHT_BLUE = "\x1F""BRIGHT_BLUE\x1F";
const std::string BRIGHT_GREEN = "\x1F""BRIGHT_GREEN\x1F";
const std::string BRIGHT_RED = "\x1F""BRIGHT_RED\x1F";
const std::string BRIGHT_YELLOW = "\x1F""BRIGHT_YELLOW\x1F";
const std::string BRIGHT_WHITE = "\x1F""BRIGHT_WHITE\x1F";
const std::string BRIGHT_CYAN = "\x1F""BRIGHT_CYAN\x1F";
const std::string BRIGHT_MAGENTA = "\x1F""BRIGHT_MAGENTA\x1F";
const std::string DEFAULT = "\x1F""DEFAULT\x1F";

const char ILogger::COLOR_DELIMETER = '\x1F';

const std::array<std::string, 6> ILogger::LEVEL_NAMES = {
  {"FATAL",
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG",
  "TRACE"}
};

}
