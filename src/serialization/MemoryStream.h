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

#pragma once


#include "IStream.h"
#include <vector>
#include <algorithm>
#include <cstring> // memcpy

namespace cryptonote {

class MemoryStream: 
  public IInputStream, 
  public IOutputStream {
public:

  MemoryStream() : 
    m_readPos(0), m_writePos(0) {}

  virtual void write(const char* data, std::size_t size) override {
    if (size == 0) {
      return;
    }

    if (m_writePos + size > m_buffer.size()) {
      m_buffer.resize(m_writePos + size);
    }

    memcpy(&m_buffer[m_writePos], data, size);
    m_writePos += size;
  }

  virtual std::size_t read(char* data, std::size_t size) override {
    size_t readSize = std::min(size, m_buffer.size() - m_readPos);
    
    if (readSize > 0) {
      memcpy(data, &m_buffer[m_readPos], readSize);
      m_readPos += readSize;
    }

    return readSize;
  }

  size_t size() {
    return m_buffer.size();
  }

  const char* data() {
    return m_buffer.data();
  }

  void clear() {
    m_readPos = 0;
    m_writePos = 0;
    m_buffer.resize(0);
  }

private:

  size_t m_readPos;
  size_t m_writePos;
  std::vector<char> m_buffer;
};

}

