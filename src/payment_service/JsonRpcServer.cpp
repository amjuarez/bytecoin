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

#include "JsonRpcServer.h"

#include <fstream>
#include <future>
#include <system_error>
#include <memory>
#include <sstream>
#include "HTTP/HttpParserErrorCodes.h"

#include <System/TcpConnection.h>
#include <System/TcpListener.h>
#include <System/TcpStream.h>
#include <System/Ipv4Address.h>
#include <System/InterruptedException.h>
#include "HTTP/HttpParser.h"
#include "HTTP/HttpResponse.h"
#include "JsonRpcMessages.h"
#include "WalletService.h"
#include "WalletServiceErrorCodes.h"

#include "Common/JsonValue.h"
#include "serialization/JsonInputValueSerializer.h"
#include "serialization/JsonOutputStreamSerializer.h"

namespace PaymentService {

JsonRpcServer::JsonRpcServer(System::Dispatcher& sys, System::Event& stopEvent, WalletService& service, Logging::ILogger& loggerGroup) :
    system(sys),
    stopEvent(stopEvent),
    service(service),
    logger(loggerGroup, "JsonRpcServer")
{
}

void JsonRpcServer::start(const Configuration& config) {
  logger(Logging::INFO) << "Starting server on " << config.bindAddress << ":" << config.bindPort;

  try {
    System::TcpListener listener(system, System::Ipv4Address(config.bindAddress), config.bindPort);
    system.spawn([this, &listener] () {this->stopEvent.wait(); listener.stop(); });
    for (;;) {
      System::TcpConnection connection = listener.accept();
      system.spawn(std::bind(&JsonRpcServer::sessionProcedure, this, new System::TcpConnection(std::move(connection))));
    }
  } catch (System::InterruptedException&) {
    logger(Logging::DEBUGGING) << "Server is stopped";
  } catch (std::exception& ex) {
    logger(Logging::FATAL) << ex.what();
  }
}

void JsonRpcServer::sessionProcedure(System::TcpConnection* tcpConnection) {
  logger(Logging::DEBUGGING) << "new connection has been accepted";
  std::unique_ptr<System::TcpConnection> connection(tcpConnection);

  System::TcpStreambuf streambuf(*connection);
  std::iostream stream(&streambuf);

  CryptoNote::HttpParser parser;

  try {
    for (;;) {
      CryptoNote::HttpRequest req;
      CryptoNote::HttpResponse resp;

      parser.receiveRequest(stream, req);
      processHttpRequest(req, resp);

      stream << resp;
      stream.flush();
    }
  } catch (std::system_error& e) {
    //todo: write error conditions
    if (e.code().category() == CryptoNote::error::HttpParserErrorCategory::INSTANCE) {
      if (e.code().value() == CryptoNote::error::END_OF_STREAM) {
        logger(Logging::DEBUGGING) << "The client is disconnected";
        return;
      }
    }
    logger(Logging::WARNING) << e.code().message();
  } catch (std::exception& e) {
    logger(Logging::WARNING) << e.what();
  }
}

void JsonRpcServer::processHttpRequest(const CryptoNote::HttpRequest& req, CryptoNote::HttpResponse& resp) {
  try {
    logger(Logging::TRACE) << "HTTP request came: \n" << req;

    if (req.getUrl() == "/json_rpc") {
      std::stringstream jsonInputStream(req.getBody());
      Common::JsonValue jsonRpcRequest;
      Common::JsonValue jsonRpcResponse(Common::JsonValue::OBJECT);

      try {
        jsonInputStream >> jsonRpcRequest;
      } catch (std::runtime_error&) {
        logger(Logging::WARNING) << "Couldn't parse request: \"" << req.getBody() << "\"";
        makeJsonParsingErrorResponse(jsonRpcResponse);
        resp.setStatus(CryptoNote::HttpResponse::STATUS_200);
        resp.setBody(jsonRpcResponse.toString());
        return;
      }

      processJsonRpcRequest(jsonRpcRequest, jsonRpcResponse);

      std::stringstream jsonOutputStream;
      jsonOutputStream << jsonRpcResponse;

      resp.setStatus(CryptoNote::HttpResponse::STATUS_200);
      resp.setBody(jsonOutputStream.str());

    } else {
      logger(Logging::WARNING) << "Requested url \"" << req.getUrl() << "\" is not found";
      resp.setStatus(CryptoNote::HttpResponse::STATUS_404);
      return;
    }
  } catch (std::exception& e) {
    logger(Logging::WARNING) << "Error while processing http request: " << e.what();
    resp.setStatus(CryptoNote::HttpResponse::STATUS_500);
  }
}

void JsonRpcServer::processJsonRpcRequest(const Common::JsonValue& req, Common::JsonValue& resp) {
  try {
    prepareJsonResponse(req, resp);

    std::string method = req("method").getString();

    CryptoNote::JsonInputValueSerializer inputSerializer;
    CryptoNote::JsonOutputStreamSerializer outputSerializer;

    inputSerializer.setJsonValue(&req("params"));

    if (method == "send_transaction") {
      SendTransactionRequest sendReq;
      SendTransactionResponse sendResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        sendReq.serialize(inputSerializer, "");
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec = service.sendTransaction(sendReq, sendResp);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      sendResp.serialize(outputSerializer, "");
    } else if (method == "get_address") {
      GetAddressResponse getAddrResp;

      std::error_code ec = service.getAddress(getAddrResp.address);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      getAddrResp.serialize(outputSerializer, "");
    } else if (method == "get_actual_balance") {
      GetActualBalanceResponse actualResp;

      std::error_code ec = service.getActualBalance(actualResp.actualBalance);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      actualResp.serialize(outputSerializer, "");
    } else if (method == "get_pending_balance") {
      GetPendingBalanceResponse pendingResp;

      std::error_code ec = service.getPendingBalance(pendingResp.pendingBalance);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      pendingResp.serialize(outputSerializer, "");
    } else if (method == "get_transactions_count") {
      GetTransactionsCountResponse txResp;

      std::error_code ec = service.getTransactionsCount(txResp.transactionsCount);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      txResp.serialize(outputSerializer, "");
    } else if (method == "get_transfers_count") {
      GetTransfersCountResponse trResp;

      std::error_code ec = service.getTransfersCount(trResp.transfersCount);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      trResp.serialize(outputSerializer, "");
    } else if (method == "get_transaction_id_by_transfer_id") {
      GetTransactionIdByTransferIdRequest getReq;
      GetTransactionIdByTransferIdResponse getResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        getReq.serialize(inputSerializer, "");
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      CryptoNote::TransactionId txId;
      std::error_code ec = service.getTransactionByTransferId(getReq.transferId, txId);
      getResp.transactionid = txId;
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      getResp.serialize(outputSerializer, "");
    } else if (method == "get_transaction") {
      GetTransactionRequest getReq;
      GetTransactionResponse getResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        getReq.serialize(inputSerializer, "");
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec = service.getTransaction(getReq.transactionId, getResp.found, getResp.transactionInfo);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      getResp.serialize(outputSerializer, "");
    } else if (method == "get_transfer") {
      GetTransferRequest getReq;
      GetTransferResponse getResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        getReq.serialize(inputSerializer, "");
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec = service.getTransfer(getReq.transferId, getResp.found, getResp.transferInfo);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      getResp.serialize(outputSerializer, "");
    } else if (method == "get_incoming_payments") {
      GetIncomingPaymentsRequest getReq;
      GetIncomingPaymentsResponse getResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        getReq.serialize(inputSerializer, "");
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      WalletService::IncomingPayments payments;
      std::error_code ec = service.getIncomingPayments(getReq.payments, payments);
      if (ec) {
        if (ec == make_error_code(PaymentService::error::REQUEST_ERROR)) {
          makeGenericErrorReponse(resp, "Invalid Request", -32600);
        } else {
          makeErrorResponse(ec, resp);
        }

        return;
      }

      for (auto p: payments) {
        PaymentsById pbid;
        pbid.id = std::move(p.first);
        pbid.payments = std::move(p.second);

        getResp.payments.push_back(std::move(pbid));
      }

      getResp.serialize(outputSerializer, "");
    } else {
      logger(Logging::DEBUGGING) << "Requested method not found: " << method;
      makeMethodNotFoundResponse(resp);
      return;
    }

    Common::JsonValue v = outputSerializer.getJsonValue();
    fillJsonResponse(v, resp);

  } catch (RequestSerializationError&) {
    logger(Logging::WARNING) << "Wrong request came";
    makeGenericErrorReponse(resp, "Invalid Request", -32600);
  } catch (std::exception& e) {
    logger(Logging::WARNING) << "Error occured while processing JsonRpc request";
    makeGenericErrorReponse(resp, e.what());
  }
}

void JsonRpcServer::prepareJsonResponse(const Common::JsonValue& req, Common::JsonValue& resp) {
  using Common::JsonValue;

  if (req.count("id")) {
    JsonValue id = req("id");
    resp.insert("id", id);
  }

  JsonValue jsonRpc;
  jsonRpc = "2.0";

  resp.insert("jsonrpc", jsonRpc);
}

void JsonRpcServer::makeErrorResponse(const std::error_code& ec, Common::JsonValue& resp) {
  using Common::JsonValue;

  JsonValue error(JsonValue::OBJECT);

  JsonValue code;
  code = static_cast<int64_t>(-32000); //Application specific error code

  JsonValue message;
  message = ec.message();

  JsonValue data(JsonValue::OBJECT);
  JsonValue appCode;
  appCode = static_cast<int64_t>(ec.value());
  data.insert("application_code", appCode);

  error.insert("code", code);
  error.insert("message", message);
  error.insert("data", data);

  resp.insert("error", error);
}

void JsonRpcServer::makeGenericErrorReponse(Common::JsonValue& resp, const char* what, int errorCode) {
  using Common::JsonValue;

  JsonValue error(JsonValue::OBJECT);

  JsonValue code;
  code = static_cast<int64_t>(errorCode);

  std::string msg;
  if (what) {
    msg = what;
  } else {
    msg = "Unknown application error";
  }

  JsonValue message;
  message = msg;

  error.insert("code", code);
  error.insert("message", message);

  resp.insert("error", error);

}

void JsonRpcServer::makeMethodNotFoundResponse(Common::JsonValue& resp) {
  using Common::JsonValue;

  JsonValue error(JsonValue::OBJECT);

  JsonValue code;
  code = static_cast<int64_t>(-32601); //ambigous declaration of JsonValue::operator= (between int and JsonValue)

  JsonValue message;
  message = "Method not found";

  error.insert("code", code);
  error.insert("message", message);

  resp.insert("error", error);
}

void JsonRpcServer::fillJsonResponse(const Common::JsonValue& v, Common::JsonValue& resp) {
  resp.insert("result", v);
}

void JsonRpcServer::makeJsonParsingErrorResponse(Common::JsonValue& resp) {
  using Common::JsonValue;

  resp = JsonValue(JsonValue::OBJECT);
  resp.insert("jsonrpc", "2.0");
  resp.insert("id", nullptr);

  JsonValue error(JsonValue::OBJECT);
  JsonValue code;
  code = static_cast<int64_t>(-32700); //ambigous declaration of JsonValue::operator= (between int and JsonValue)

  JsonValue message;
  message = "Parse error";

  error.insert("code", code);
  error.insert("message", message);

  resp.insert("error", error);
}

}
