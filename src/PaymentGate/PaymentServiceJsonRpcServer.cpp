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

#include "PaymentServiceJsonRpcServer.h"

#include "PaymentServiceJsonRpcMessages.h"
#include "WalletService.h"

#include "Common/JsonValue.h"
#include "Serialization/JsonInputValueSerializer.h"
#include "Serialization/JsonOutputStreamSerializer.h"

namespace PaymentService {

PaymentServiceJsonRpcServer::PaymentServiceJsonRpcServer(System::Dispatcher& sys, System::Event& stopEvent, WalletService& service, Logging::ILogger& loggerGroup) 
  : JsonRpcServer(sys, stopEvent, loggerGroup)
  , service(service)
  , logger(loggerGroup, "PaymentServiceJsonRpcServer")
{
}

void PaymentServiceJsonRpcServer::processJsonRpcRequest(const Common::JsonValue& req, Common::JsonValue& resp) {
  try {
    prepareJsonResponse(req, resp);

    std::string method = req("method").getString();

    CryptoNote::JsonOutputStreamSerializer outputSerializer;

    if (method == "send_transaction") {
      SendTransactionRequest sendReq;
      SendTransactionResponse sendResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(req("params"));
        serialize(sendReq, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec = service.sendTransaction(sendReq, sendResp);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(sendResp, outputSerializer);
    } else if (method == "get_address") {
      GetAddressRequest getAddrReq;
      GetAddressResponse getAddrResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(req("params"));
        serialize(getAddrReq, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec = service.getAddress(getAddrReq.index, getAddrResp.address);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(getAddrResp, outputSerializer);
    } else if (method == "create_address") {
      CreateAddressResponse createAddrResp;

      std::error_code ec = service.createAddress(createAddrResp.address);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(createAddrResp, outputSerializer);
    } else if (method == "get_address_count") {
      GetAddressCountResponse addressCountResp;

      std::error_code ec = service.getAddressCount(addressCountResp.count);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(addressCountResp, outputSerializer);
    } else if (method == "delete_address") {
      DeleteAddressRequest delAddrReq;
      DeleteAddressResponse delAddrResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(req("params"));
        serialize(delAddrReq, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec = service.deleteAddress(delAddrReq.address);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(delAddrResp, outputSerializer);
    } else if (method == "get_actual_balance") {
      GetActualBalanceRequest actualReq;
      GetActualBalanceResponse actualResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(req("params"));
        serialize(actualReq, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec;
      if (actualReq.address == "") {
        ec = service.getActualBalance(actualResp.actualBalance);
      } else {
        ec = service.getActualBalance(actualReq.address, actualResp.actualBalance);
      }

      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(actualResp, outputSerializer);
    } else if (method == "get_pending_balance") {
      GetPendingBalanceRequest pendingReq;
      GetPendingBalanceResponse pendingResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(req("params"));
        serialize(pendingReq, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec;
      if (pendingReq.address == "") {
        ec = service.getPendingBalance(pendingResp.pendingBalance);
      } else {
        ec = service.getPendingBalance(pendingReq.address, pendingResp.pendingBalance);
      }

      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(pendingResp, outputSerializer);
    } else if (method == "get_transactions_count") {
      GetTransactionsCountResponse txResp;

      std::error_code ec = service.getTransactionsCount(txResp.transactionsCount);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(txResp, outputSerializer);
    } else if (method == "get_transfers_count") {
      GetTransfersCountResponse trResp;

      std::error_code ec = service.getTransfersCount(trResp.transfersCount);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(trResp, outputSerializer);
    } else if (method == "get_transaction_id_by_transfer_id") {
      GetTransactionIdByTransferIdRequest getReq;
      GetTransactionIdByTransferIdResponse getResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(req("params"));
        serialize(getReq, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      size_t txId;
      std::error_code ec = service.getTransactionByTransferId(getReq.transferId, txId);
      getResp.transactionid = txId;
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(getResp, outputSerializer);
    } else if (method == "get_transaction") {
      GetTransactionRequest getReq;
      GetTransactionResponse getResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(req("params"));
        serialize(getReq, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec = service.getTransaction(getReq.transactionId, getResp.found, getResp.transactionInfo);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(getResp, outputSerializer);
    } else if (method == "list_transactions") {
      ListTransactionsRequest listReq;
      ListTransactionsResponse listResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(req("params"));
        serialize(listReq, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec = service.listTransactions(static_cast<size_t>(listReq.startingTransactionId), listReq.maxTransactionCount, listResp.transactions);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(listResp, outputSerializer);
    } else if (method == "get_transfer") {
      GetTransferRequest getReq;
      GetTransferResponse getResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(req("params"));
        serialize(getReq, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      std::error_code ec = service.getTransfer(getReq.transferId, getResp.found, getResp.transferInfo);
      if (ec) {
        makeErrorResponse(ec, resp);
        return;
      }

      serialize(getResp, outputSerializer);
    } else if (method == "get_incoming_payments") {
      GetIncomingPaymentsRequest getReq;
      GetIncomingPaymentsResponse getResp;

      //XXX: refactor it when migrate to different exception types in different subsystems!
      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(req("params"));
        serialize(getReq, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(resp, "Invalid Request", -32600);
        return;
      }

      WalletService::IncomingPayments payments;
      std::error_code ec = service.getIncomingPayments(getReq.payments, payments);
      if (ec) {
        if (ec == make_error_code(std::errc::argument_out_of_domain)) {
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

      serialize(getResp, outputSerializer);
    } else {
      logger(Logging::DEBUGGING) << "Requested method not found: " << method;
      makeMethodNotFoundResponse(resp);
      return;
    }

    fillJsonResponse(outputSerializer.getValue(), resp);

  } catch (RequestSerializationError&) {
    logger(Logging::WARNING) << "Wrong request came";
    makeGenericErrorReponse(resp, "Invalid Request", -32600);
  } catch (std::exception& e) {
    logger(Logging::WARNING) << "Error occurred while processing JsonRpc request: " << e.what();
    makeGenericErrorReponse(resp, e.what());
  }
}

}
