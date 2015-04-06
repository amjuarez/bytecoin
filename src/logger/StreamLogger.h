#pragma once

#include <mutex>
#include "CommonLogger.h"

namespace Log {

class StreamLogger : public CommonLogger {
public:
  StreamLogger(std::ostream& stream, ILogger::Level level = ILogger::DEBUGGING);

protected:
  virtual void doLogString(Level level, boost::posix_time::ptime time, const std::string& message) override;

private:
  std::mutex mutex;
  std::ostream& stream;
};

}
