// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memory>

#include <HTTP/HttpRequest.h>
#include <HTTP/HttpResponse.h>
#include <System/TcpConnection.h>
#include <System/TcpStream.h>

#include "Serialization/SerializationTools.h"

namespace CryptoNote {

class ConnectException : public std::runtime_error  {
public:
  ConnectException(const std::string& whatArg);
};

class HttpClient {
public:

  HttpClient(System::Dispatcher& dispatcher, const std::string& address, uint16_t port);
  ~HttpClient();
  void request(const HttpRequest& req, HttpResponse& res);
  
  bool isConnected() const;

private:
  void connect();
  void disconnect();

  const std::string m_address;
  const uint16_t m_port;

  bool m_connected = false;
  System::Dispatcher& m_dispatcher;
  System::TcpConnection m_connection;
  std::unique_ptr<System::TcpStreambuf> m_streamBuf;
};

template <typename Request, typename Response>
void invokeJsonCommand(HttpClient& client, const std::string& url, const Request& req, Response& res) {
  HttpRequest hreq;
  HttpResponse hres;

  hreq.setUrl(url);
  hreq.setBody(storeToJson(req));
  client.request(hreq, hres);

  if (hres.getStatus() != HttpResponse::STATUS_200) {
    throw std::runtime_error("HTTP status: " + std::to_string(hres.getStatus()));
  }

  if (!loadFromJson(res, hres.getBody())) {
    throw std::runtime_error("Failed to parse JSON response");
  }
}

template <typename Request, typename Response>
void invokeBinaryCommand(HttpClient& client, const std::string& url, const Request& req, Response& res) {
  HttpRequest hreq;
  HttpResponse hres;

  hreq.setUrl(url);
  hreq.setBody(storeToBinaryKeyValue(req));
  client.request(hreq, hres);

  if (!loadFromBinaryKeyValue(res, hres.getBody())) {
    throw std::runtime_error("Failed to parse binary response");
  }
}

}
