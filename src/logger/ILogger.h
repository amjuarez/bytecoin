#pragma once

#include <string>
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>

#undef ERROR

namespace Log {

class ILogger {
public:
  typedef std::size_t Level;

  const static Level FATAL = 0;
  const static Level ERROR = 1;
  const static Level WARNING = 2;
  const static Level INFO = 3;
  const static Level DEBUGGING = 4;
  const static Level TRACE = 5;

  const static std::string BLUE;
  const static std::string GREEN;
  const static std::string RED;
  const static std::string YELLOW;
  const static std::string WHITE;
  const static std::string CYAN;
  const static std::string MAGENTA;
  const static std::string BRIGHT_BLUE;
  const static std::string BRIGHT_GREEN;
  const static std::string BRIGHT_RED;
  const static std::string BRIGHT_YELLOW;
  const static std::string BRIGHT_WHITE;
  const static std::string BRIGHT_CYAN;
  const static std::string BRIGHT_MAGENTA;
  const static std::string DEFAULT;

  const static char COLOR_DELIMETER;

  const static std::vector<std::string> LEVEL_NAMES;

  virtual void enableCategory(const std::string& category) = 0;
  virtual void disableCategory(const std::string& category) = 0;
  virtual void setMaxLevel(Level level) = 0;
  virtual void operator()(const std::string& category, Level level, boost::posix_time::ptime time, const std::string& body) = 0;
};

}
