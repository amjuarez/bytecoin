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

#include "JsonRpcServer/JsonRpcServer.h"

namespace PaymentService {

class WalletService;

class PaymentServiceJsonRpcServer : public CryptoNote::JsonRpcServer {
public:
  PaymentServiceJsonRpcServer(System::Dispatcher& sys, System::Event& stopEvent, WalletService& service, Logging::ILogger& loggerGroup);
  PaymentServiceJsonRpcServer(const PaymentServiceJsonRpcServer&) = delete;

protected:
  virtual void processJsonRpcRequest(const Common::JsonValue& req, Common::JsonValue& resp) override;

private:
  WalletService& service;
  Logging::LoggerRef logger;
};

} //namespace PaymentService
