// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <streambuf>
#include <array>

#include <System/TcpConnection.h>

namespace System {

class TcpStreambuf : public std::streambuf {
public:
  TcpStreambuf(TcpConnection& connection);
  TcpStreambuf(const TcpStreambuf&) = delete;

  virtual ~TcpStreambuf();

private:
  std::streambuf::int_type underflow() override;
  std::streambuf::int_type overflow(std::streambuf::int_type ch) override;
  int sync() override;

  bool dumpBuffer();

  TcpConnection& connection;

  std::array<char, 4096> readBuf;
  std::array<uint8_t, /*1024*/ 16> writeBuf;
};

}
