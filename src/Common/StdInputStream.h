// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <istream>
#include "IInputStream.h"

namespace Common {

class StdInputStream : public IInputStream {
public:
  StdInputStream(std::istream& in);
  StdInputStream& operator=(const StdInputStream&) = delete;
  size_t readSome(void* data, size_t size) override;

private:
  std::istream& in;
};

}
