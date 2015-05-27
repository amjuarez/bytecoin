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

#pragma once

#include "serialization/ISerializer.h"
#include <vector>
#include <exception>

namespace PaymentService {

class RequestSerializationError: public std::exception {
public:
  virtual const char* what() const throw() override { return "Request error"; }
};

struct TransferDestination {
  uint64_t amount;
  std::string address;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct SendTransactionRequest {
  SendTransactionRequest() : unlockTime(0) {}
  std::vector<TransferDestination> destinations;
  uint64_t fee;
  uint64_t mixin;
  uint64_t unlockTime;
  std::string paymentId;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct SendTransactionResponse {
  uint64_t transactionId;
  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetAddressResponse {
  std::string address;
  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetActualBalanceResponse {
  uint64_t actualBalance;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetPendingBalanceResponse {
  uint64_t pendingBalance;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetTransactionsCountResponse {
  uint64_t transactionsCount;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetTransfersCountResponse {
  uint64_t transfersCount;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetTransactionIdByTransferIdRequest {
  uint64_t transferId;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetTransactionIdByTransferIdResponse {
  uint64_t transactionid;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetTransactionRequest {
  uint64_t transactionId;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct TransactionRpcInfo {
  uint64_t firstTransferId;
  uint64_t transferCount;
  int64_t totalAmount;
  uint64_t fee;
  std::string hash;
  bool isCoinbase;
  uint64_t blockHeight;
  uint64_t timestamp;
  std::string extra;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetTransactionResponse {
  bool found;
  TransactionRpcInfo transactionInfo;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct TransferRpcInfo {
  std::string address;
  int64_t amount;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetTransferRequest {
  uint64_t transferId;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetTransferResponse {
  bool found;
  TransferRpcInfo transferInfo;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetIncomingPaymentsRequest {
  std::vector<std::string> payments;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct PaymentDetails
{
  std::string txHash;
  uint64_t amount;
  uint64_t blockHeight;
  uint64_t unlockTime;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct PaymentsById {
  std::string id;
  std::vector<PaymentDetails> payments;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

struct GetIncomingPaymentsResponse {
  std::vector<PaymentsById> payments;

  void serialize(CryptoNote::ISerializer& serializer, const std::string& name);
};

} //namespace PaymentService
