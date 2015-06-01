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

#include <memory>

#include <HTTP/HttpRequest.h>
#include <HTTP/HttpResponse.h>
#include <System/TcpConnection.h>
#include <System/TcpStream.h>

// epee serialization
#include "misc_log_ex.h"
#include "storages/portable_storage_template_helper.h"

namespace CryptoNote {

class HttpClient {
public:

  HttpClient(System::Dispatcher& dispatcher, const std::string& address, uint16_t port);
  void request(const HttpRequest& req, HttpResponse& res);

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
  hreq.setBody(epee::serialization::store_t_to_json(req));
  client.request(hreq, hres);

  if (hres.getStatus() != HttpResponse::STATUS_200) {
    throw std::runtime_error("HTTP status: " + std::to_string(hres.getStatus()));
  }

  if (!epee::serialization::load_t_from_json(res, hres.getBody())) {
    throw std::runtime_error("Failed to parse JSON response");
  }
}

template <typename Request, typename Response>
void invokeBinaryCommand(HttpClient& client, const std::string& url, const Request& req, Response& res) {
  HttpRequest hreq;
  HttpResponse hres;

  hreq.setUrl(url);
  hreq.setBody(epee::serialization::store_t_to_binary(req));
  client.request(hreq, hres);

  if (!epee::serialization::load_t_from_binary(res, hres.getBody())) {
    throw std::runtime_error("Failed to parse binary response");
  }
}

}
