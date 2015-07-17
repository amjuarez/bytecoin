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

#include <system_error>

#include <System/Dispatcher.h>
#include <System/Event.h>
#include "Logging/ILogger.h"
#include "Logging/LoggerRef.h"
#include "rpc/HttpServer.h"

#include "PaymentServiceConfiguration.h"


namespace CryptoNote {
class HttpResponse;
class HttpRequest;
}

namespace Common {
class JsonValue;
}

namespace System {
class TcpConnection;
}

namespace PaymentService {

class WalletService;

class JsonRpcServer : CryptoNote::HttpServer {
public:
  JsonRpcServer(System::Dispatcher& sys, System::Event& stopEvent, WalletService& service, Logging::ILogger& loggerGroup);
  JsonRpcServer(const JsonRpcServer&) = delete;

  void start(const Configuration& config);

private:
  void sessionProcedure(System::TcpConnection* tcpConnection);

  // HttpServer
  virtual void processRequest(const CryptoNote::HttpRequest& request, CryptoNote::HttpResponse& response) override;

  void processJsonRpcRequest(const Common::JsonValue& req, Common::JsonValue& resp);
  void prepareJsonResponse(const Common::JsonValue& req, Common::JsonValue& resp);

  void makeErrorResponse(const std::error_code& ec, Common::JsonValue& resp);
  void makeMethodNotFoundResponse(Common::JsonValue& resp);
  void makeGenericErrorReponse(Common::JsonValue& resp, const char* what, int errorCode = -32001);
  void makeJsonParsingErrorResponse(Common::JsonValue& resp);

  void fillJsonResponse(const Common::JsonValue& v, Common::JsonValue& resp);

  System::Dispatcher& system;
  System::Event& stopEvent;
  WalletService& service;
  Logging::LoggerRef logger;
};

} //namespace PaymentService
