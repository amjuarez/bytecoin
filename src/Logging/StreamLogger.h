// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <mutex>
#include "CommonLogger.h"

namespace Logging {

class StreamLogger : public CommonLogger {
public:
  StreamLogger(Level level = DEBUGGING);
  StreamLogger(std::ostream& stream, Level level = DEBUGGING);
  void attachToStream(std::ostream& stream);

protected:
  virtual void doLogString(const std::string& message) override;

protected:
  std::ostream* stream;

private:
  std::mutex mutex;
};

}
