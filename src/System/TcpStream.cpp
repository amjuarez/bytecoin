// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "TcpStream.h"

#include <cstring>

using namespace System;

TcpStreambuf::TcpStreambuf(TcpConnection& connection) : connection(connection) {
  setg(&readBuf.front(), &readBuf.front(), &readBuf.front());
  setp(reinterpret_cast<char*>(&writeBuf.front()), reinterpret_cast<char *>(&writeBuf.front() + writeBuf.max_size()));
}

TcpStreambuf::~TcpStreambuf() {
  dumpBuffer();
}

std::streambuf::int_type TcpStreambuf::underflow() {
  if (gptr() < egptr())
    return traits_type::to_int_type(*gptr());

  size_t bytesRead;
  try {
    bytesRead = connection.read(reinterpret_cast<uint8_t*>(&readBuf.front()), readBuf.max_size());
  } catch (std::exception& ex) {
    return traits_type::eof();
  }

  if (bytesRead == 0) {
    return traits_type::eof();
  }

  setg(&readBuf.front(), &readBuf.front(), &readBuf.front() + bytesRead);

  return traits_type::to_int_type(*gptr());
}

int TcpStreambuf::sync() {
  return dumpBuffer() ? 0 : -1;
}

bool TcpStreambuf::dumpBuffer() {
  try {
    size_t count = pptr() - pbase();
    connection.write(&writeBuf.front(), count);
    pbump(-count);
  } catch (std::exception&) {
    return false;
  }

  return true;
}

std::streambuf::int_type TcpStreambuf::overflow(std::streambuf::int_type ch) {
  if (ch == traits_type::eof()) {
    return traits_type::eof();
  }

  if (pptr() == epptr()) {
    if (!dumpBuffer()) {
      return traits_type::eof();
    }
  }

  *pptr() = ch;
  pbump(1);

  return ch;
}
