// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include "serialization/KVBinaryInputStreamSerializer.h"
#include "serialization/KVBinaryOutputStreamSerializer.h"

namespace System {
class TcpConnection;
}

namespace CryptoNote {

enum class LevinError: int32_t {
  OK = 0,
  ERROR_CONNECTION = -1,
  ERROR_CONNECTION_NOT_FOUND = -2,
  ERROR_CONNECTION_DESTROYED = -3,
  ERROR_CONNECTION_TIMEDOUT = -4,
  ERROR_CONNECTION_NO_DUPLEX_PROTOCOL = -5,
  ERROR_CONNECTION_HANDLER_NOT_DEFINED = -6,
  ERROR_FORMAT = -7,
};

class LevinProtocol {
public:

  LevinProtocol(System::TcpConnection& connection);

  template <typename Req, typename Resp>
  void invoke(uint32_t command, const Req& req, Resp& resp, bool readResponse = true) {
    decode(sendBuf(command, encode(req), true, readResponse), resp);
  }

  template <typename Req>
  void notify(uint32_t command, const Req& req, int) {
    sendBuf(command, encode(req), false, false);
  }

  struct Command {
    uint32_t command;
    bool isNotify;
    bool isResponse;
    std::string buf;

    bool needReply() const {
      return !(isNotify || isResponse);
    }
  };

  bool readCommand(Command& cmd);

  std::string sendBuf(uint32_t command, const std::string& out, bool needResponse, bool readResponse = false);
  void sendReply(uint32_t command, const std::string& out, int32_t returnCode);

  template <typename T>
  static bool decode(const std::string& buf, T& value) {
    try {
      std::stringstream stream(buf);
      KVBinaryInputStreamSerializer serializer(stream);
      serialize(value, serializer);
    } catch (std::exception&) {
      return false;
    }

    return true;
  }

  template <typename T>
  static std::string encode(const T& value) {
    KVBinaryOutputStreamSerializer serializer;
    serialize(const_cast<T&>(value), serializer);
    std::stringstream stream;
    serializer.write(stream);
    return stream.str();
  }

private:

  bool readStrict(void* ptr, size_t size);
  System::TcpConnection& m_conn;
};

}
