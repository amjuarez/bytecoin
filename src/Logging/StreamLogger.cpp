// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "StreamLogger.h"
#include <iostream>
#include <sstream>

namespace Logging {

StreamLogger::StreamLogger(Level level) : CommonLogger(level), stream(nullptr) {
}

StreamLogger::StreamLogger(std::ostream& stream, Level level) : CommonLogger(level), stream(&stream) {
}

void StreamLogger::attachToStream(std::ostream& stream) {
  this->stream = &stream;
}

void StreamLogger::doLogString(const std::string& message) {
  if (stream != nullptr && stream->good()) {
    std::lock_guard<std::mutex> lock(mutex);
    bool readingText = true;
    for (size_t charPos = 0; charPos < message.size(); ++charPos) {
      if (message[charPos] == ILogger::COLOR_DELIMETER) {
        readingText = !readingText;
      } else if (readingText) {
        *stream << message[charPos];
      }
    }

    *stream << std::flush;
  }
}

}
