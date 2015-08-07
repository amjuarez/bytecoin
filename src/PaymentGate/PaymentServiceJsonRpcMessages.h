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

#include <exception>
#include <vector>

#include "Serialization/ISerializer.h"

namespace PaymentService {

class RequestSerializationError: public std::exception {
public:
  virtual const char* what() const throw() override { return "Request error"; }
};

struct TransferDestination {
  uint64_t amount;
  std::string address;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct SendTransactionRequest {
  SendTransactionRequest() : unlockTime(0) {}

  std::vector<TransferDestination> destinations;
  uint64_t fee;
  uint64_t mixin;
  uint64_t unlockTime;
  std::string paymentId;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct SendTransactionResponse {
  uint64_t transactionId;
  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetAddressRequest {
  GetAddressRequest() : index(0) {}

  size_t index;
  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetAddressCountResponse {
  std::size_t count;
  void serialize(CryptoNote::ISerializer& serializer);
};

struct DeleteAddressRequest {
  std::string address;
  void serialize(CryptoNote::ISerializer& serializer);
};

struct DeleteAddressResponse {
  void serialize(CryptoNote::ISerializer& serializer);
};

struct CreateAddressResponse {
  std::string address;
  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetAddressResponse {
  std::string address;
  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetActualBalanceRequest {
  std::string address;
  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetActualBalanceResponse {
  uint64_t actualBalance;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetPendingBalanceRequest {
  std::string address;
  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetPendingBalanceResponse {
  uint64_t pendingBalance;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransactionsCountResponse {
  uint64_t transactionsCount;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransfersCountResponse {
  uint64_t transfersCount;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransactionIdByTransferIdRequest {
  uint64_t transferId;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransactionIdByTransferIdResponse {
  uint64_t transactionid;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransactionRequest {
  uint64_t transactionId;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct TransferRpcInfo {
  std::string address;
  int64_t amount;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct TransactionRpcInfo {
  uint64_t firstTransferId;
  uint64_t transferCount;
  int64_t totalAmount;
  uint64_t fee;
  std::string hash;
  uint64_t blockHeight;
  uint64_t timestamp;
  std::string extra;
  std::vector<TransferRpcInfo> transfers;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransactionResponse {
  bool found;
  TransactionRpcInfo transactionInfo;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct ListTransactionsRequest {
  uint32_t startingTransactionId;
  uint32_t maxTransactionCount;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct ListTransactionsResponse {
  std::vector<TransactionRpcInfo> transactions;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransferRequest {
  uint64_t transferId;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransferResponse {
  bool found;
  TransferRpcInfo transferInfo;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetIncomingPaymentsRequest {
  std::vector<std::string> payments;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct PaymentDetails
{
  std::string txHash;
  uint64_t amount;
  uint64_t blockHeight;
  uint64_t unlockTime;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct PaymentsById {
  std::string id;
  std::vector<PaymentDetails> payments;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetIncomingPaymentsResponse {
  std::vector<PaymentsById> payments;

  void serialize(CryptoNote::ISerializer& serializer);
};

} //namespace PaymentService
