#include "ILogger.h"

using namespace Log;

const std::string ILogger::BLUE = "\x1F""BLUE\x1F";
const std::string ILogger::GREEN = "\x1F""GREEN\x1F";
const std::string ILogger::RED = "\x1F""RED\x1F";
const std::string ILogger::YELLOW = "\x1F""YELLOW\x1F";
const std::string ILogger::WHITE = "\x1F""WHITE\x1F";
const std::string ILogger::CYAN = "\x1F""CYAN\x1F";
const std::string ILogger::MAGENTA = "\x1F""MAGENTA\x1F";
const std::string ILogger::BRIGHT_BLUE = "\x1F""BRIGHT_BLUE\x1F";
const std::string ILogger::BRIGHT_GREEN = "\x1F""BRIGHT_GREEN\x1F";
const std::string ILogger::BRIGHT_RED = "\x1F""BRIGHT_RED\x1F";
const std::string ILogger::BRIGHT_YELLOW = "\x1F""BRIGHT_YELLOW\x1F";
const std::string ILogger::BRIGHT_WHITE = "\x1F""BRIGHT_WHITE\x1F";
const std::string ILogger::BRIGHT_CYAN = "\x1F""BRIGHT_CYAN\x1F";
const std::string ILogger::BRIGHT_MAGENTA = "\x1F""BRIGHT_MAGENTA\x1F";
const std::string ILogger::DEFAULT = "\x1F""DEFAULT\x1F";

const char ILogger::COLOR_DELIMETER = '\x1F';

const std::vector<std::string> ILogger::LEVEL_NAMES = {
  " [FATAL] ",
  " [ERROR] ",
  " [WARNING] ",
  " [INFO] ",
  " [DEBUG] ",
  " [TRACE] "
};
