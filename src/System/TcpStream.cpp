// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

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
