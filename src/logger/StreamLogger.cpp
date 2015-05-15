#include "StreamLogger.h"
#include <iostream>
#include <sstream>

using namespace Log;

StreamLogger::StreamLogger(std::ostream& stream, ILogger::Level level) : CommonLogger(level), stream(stream) {
}

void StreamLogger::doLogString(Level level, boost::posix_time::ptime time, const std::string& message) {
  std::string result;
  std::stringstream ss(message);
  char c;
  bool readingText = true;
  while (ss.read(&c, 1)) {
    if (c == ILogger::COLOR_DELIMETER) {
      readingText = !readingText;
      continue;
    }

    if (readingText) {
      result += c;
    }
  }

  std::lock_guard<std::mutex> lock(mutex);
  stream << boost::posix_time::to_iso_extended_string(time) << ILogger::LEVEL_NAMES[level] << result << std::flush;
}
