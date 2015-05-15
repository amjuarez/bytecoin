#pragma once

#include <set>
#include "ILogger.h"

namespace Log {

class CommonLogger : public ILogger {
public:
  virtual void enableCategory(const std::string& category) override;
  virtual void disableCategory(const std::string& category) override;
  virtual void setMaxLevel(Level level) override;
  virtual void operator()(const std::string& category, Level level, boost::posix_time::ptime time, const std::string& body) override;

protected:
  std::set<std::string> disabledCategories;
  Level logLevel;

  CommonLogger(ILogger::Level level);
  virtual void doLogString(Level level, boost::posix_time::ptime time, const std::string& message);
};

}
