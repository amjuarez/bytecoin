// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "StreamTools.h"
#include <stdexcept>
#include "IInputStream.h"
#include "IOutputStream.h"

namespace Common {

void read(IInputStream& in, void* data, size_t size) {
  while (size > 0) {
    size_t readSize = in.readSome(data, size);
    if (readSize == 0) {
      throw std::runtime_error("Failed to read from IInputStream");
    }

    data = static_cast<uint8_t*>(data) + readSize;
    size -= readSize;
  }
}

void read(IInputStream& in, int8_t& value) {
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, int16_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, int32_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, int64_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, uint8_t& value) {
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, uint16_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, uint32_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, uint64_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, std::vector<uint8_t>& data, size_t size) {
  data.resize(size);
  read(in, data.data(), size);
}

void read(IInputStream& in, std::string& data, size_t size) {
  std::vector<char> temp(size);
  read(in, temp.data(), size);
  data.assign(temp.data(), size);
}

void readVarint(IInputStream& in, uint8_t& value) {
  uint8_t temp = 0;
  for (uint8_t shift = 0;; shift += 7) {
    uint8_t piece;
    read(in, piece);
    if (shift >= sizeof(temp) * 8 - 7 && piece >= 1 << (sizeof(temp) * 8 - shift)) {
      throw std::runtime_error("readVarint, value overflow");
    }

    temp |= static_cast<size_t>(piece & 0x7f) << shift;
    if ((piece & 0x80) == 0) {
      if (piece == 0 && shift != 0) {
        throw std::runtime_error("readVarint, invalid value representation");
      }

      break;
    }
  }

  value = temp;
}

void readVarint(IInputStream& in, uint16_t& value) {
  uint16_t temp = 0;
  for (uint8_t shift = 0;; shift += 7) {
    uint8_t piece;
    read(in, piece);
    if (shift >= sizeof(temp) * 8 - 7 && piece >= 1 << (sizeof(temp) * 8 - shift)) {
      throw std::runtime_error("readVarint, value overflow");
    }

    temp |= static_cast<size_t>(piece & 0x7f) << shift;
    if ((piece & 0x80) == 0) {
      if (piece == 0 && shift != 0) {
        throw std::runtime_error("readVarint, invalid value representation");
      }

      break;
    }
  }

  value = temp;
}

void readVarint(IInputStream& in, uint32_t& value) {
  uint32_t temp = 0;
  for (uint8_t shift = 0;; shift += 7) {
    uint8_t piece;
    read(in, piece);
    if (shift >= sizeof(temp) * 8 - 7 && piece >= 1 << (sizeof(temp) * 8 - shift)) {
      throw std::runtime_error("readVarint, value overflow");
    }

    temp |= static_cast<size_t>(piece & 0x7f) << shift;
    if ((piece & 0x80) == 0) {
      if (piece == 0 && shift != 0) {
        throw std::runtime_error("readVarint, invalid value representation");
      }

      break;
    }
  }

  value = temp;
}

void readVarint(IInputStream& in, uint64_t& value) {
  uint64_t temp = 0;
  for (uint8_t shift = 0;; shift += 7) {
    uint8_t piece;
    read(in, piece);
    if (shift >= sizeof(temp) * 8 - 7 && piece >= 1 << (sizeof(temp) * 8 - shift)) {
      throw std::runtime_error("readVarint, value overflow");
    }

    temp |= static_cast<uint64_t>(piece & 0x7f) << shift;
    if ((piece & 0x80) == 0) {
      if (piece == 0 && shift != 0) {
        throw std::runtime_error("readVarint, invalid value representation");
      }

      break;
    }
  }

  value = temp;
}

void write(IOutputStream& out, const void* data, size_t size) {
  while (size > 0) {
    size_t writtenSize = out.writeSome(data, size);
    if (writtenSize == 0) {
      throw std::runtime_error("Failed to write to IOutputStream");
    }

    data = static_cast<const uint8_t*>(data) + writtenSize;
    size -= writtenSize;
  }
}

void write(IOutputStream& out, int8_t value) {
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, int16_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, int32_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, int64_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, uint8_t value) {
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, uint16_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, uint32_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, uint64_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, const std::vector<uint8_t>& data) {
  write(out, data.data(), data.size());
}

void write(IOutputStream& out, const std::string& data) {
  write(out, data.data(), data.size());
}

void writeVarint(IOutputStream& out, uint32_t value) {
  while (value >= 0x80) {
    write(out, static_cast<uint8_t>(value | 0x80));
    value >>= 7;
  }

  write(out, static_cast<uint8_t>(value));
}

void writeVarint(IOutputStream& out, uint64_t value) {
  while (value >= 0x80) {
    write(out, static_cast<uint8_t>(value | 0x80));
    value >>= 7;
  }

  write(out, static_cast<uint8_t>(value));
}

}
