// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "HttpServer.h"
#include <boost/scope_exit.hpp>

#include <HTTP/HttpParser.h>
#include <System/InterruptedException.h>
#include <System/TcpStream.h>
#include <System/Ipv4Address.h>

using namespace Logging;

namespace {
std::string base64Encode(const std::string& data) {
  static const char* encodingTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const size_t resultSize = 4 * ((data.size() + 2) / 3);
  std::string result;
  result.reserve(resultSize);

  for (size_t i = 0; i < data.size(); i += 3) {
    size_t a = static_cast<size_t>(data[i]);
    size_t b = i + 1 < data.size() ? static_cast<size_t>(data[i + 1]) : 0;
    size_t c = i + 2 < data.size() ? static_cast<size_t>(data[i + 2]) : 0;

    result.push_back(encodingTable[a >> 2]);
    result.push_back(encodingTable[((a & 0x3) << 4) | (b >> 4)]);
    if (i + 1 < data.size()) {
      result.push_back(encodingTable[((b & 0xF) << 2) | (c >> 6)]);
      if (i + 2 < data.size()) {
        result.push_back(encodingTable[c & 0x3F]);
      }
    }
  }

  while (result.size() != resultSize) {
    result.push_back('=');
  }

  return result;
}

void fillUnauthorizedResponse(CryptoNote::HttpResponse& response) {
  response.setStatus(CryptoNote::HttpResponse::STATUS_401);
  response.addHeader("WWW-Authenticate", "Basic realm=\"RPC\"");
  response.addHeader("Content-Type", "text/plain");
  response.setBody("Authorization required");
}

}

namespace CryptoNote {

HttpServer::HttpServer(System::Dispatcher& dispatcher, Logging::ILogger& log)
  : m_dispatcher(dispatcher), workingContextGroup(dispatcher), logger(log, "HttpServer") {

}

void HttpServer::start(const std::string& address, uint16_t port, const std::string& user, const std::string& password) {
  m_listener = System::TcpListener(m_dispatcher, System::Ipv4Address(address), port);
  workingContextGroup.spawn(std::bind(&HttpServer::acceptLoop, this));

  if (!user.empty() || !password.empty()) {
    m_credentials = base64Encode(user + ":" + password);
  }
}

void HttpServer::stop() {
  workingContextGroup.interrupt();
  workingContextGroup.wait();
}

void HttpServer::acceptLoop() {
  try {
    System::TcpConnection connection;
    bool accepted = false;

    while (!accepted) {
      try {
        connection = m_listener.accept();
        accepted = true;
      } catch (System::InterruptedException&) {
        throw;
      } catch (std::exception&) {
        // try again
      }
    }

    m_connections.insert(&connection);
    BOOST_SCOPE_EXIT_ALL(this, &connection) { 
      m_connections.erase(&connection); };

    auto addr = connection.getPeerAddressAndPort();

    logger(DEBUGGING) << "Incoming connection from " << addr.first.toDottedDecimal() << ":" << addr.second;

    workingContextGroup.spawn(std::bind(&HttpServer::acceptLoop, this));

    System::TcpStreambuf streambuf(connection);
    std::iostream stream(&streambuf);
    HttpParser parser;

    for (;;) {
      HttpRequest req;
      HttpResponse resp;
      resp.addHeader("Access-Control-Allow-Origin", "*");

      parser.receiveRequest(stream, req);
      if (authenticate(req)) {
        processRequest(req, resp);
      } else {
        logger(WARNING) << "Authorization required " << addr.first.toDottedDecimal() << ":" << addr.second;
        fillUnauthorizedResponse(resp);
      }

      stream << resp;
      stream.flush();

      if (stream.peek() == std::iostream::traits_type::eof()) {
        break;
      }
    }

    logger(DEBUGGING) << "Closing connection from " << addr.first.toDottedDecimal() << ":" << addr.second << " total=" << m_connections.size();

  } catch (System::InterruptedException&) {
  } catch (std::exception& e) {
    logger(WARNING) << "Connection error: " << e.what();
  }
}

bool HttpServer::authenticate(const HttpRequest& request) const {
  if (!m_credentials.empty()) {
    auto headerIt = request.getHeaders().find("authorization");
    if (headerIt == request.getHeaders().end()) {
      return false;
    }

    if (headerIt->second.substr(0, 6) != "Basic ") {
      return false;
    }

    if (headerIt->second.substr(6) != m_credentials) {
      return false;
    }
  }

  return true;
}

}
