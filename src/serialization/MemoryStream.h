// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

