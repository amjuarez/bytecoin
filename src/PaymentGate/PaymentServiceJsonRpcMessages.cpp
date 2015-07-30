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

#include "PaymentServiceJsonRpcMessages.h"
#include "Serialization/SerializationOverloads.h"

namespace PaymentService {

void TransferDestination::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(amount, "amount");
  r &= serializer(address, "address");

  if (!r) {
    throw RequestSerializationError();
  }
}

void SendTransactionRequest::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(destinations, "destinations");
  r &= serializer(fee, "fee");
  r &= serializer(mixin, "mixin");
  serializer(unlockTime, "unlock_time");
  serializer(paymentId, "payment_id");

  if (!r) {
    throw RequestSerializationError();
  }
}

void SendTransactionResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(transactionId, "transaction_id");
}

void GetAddressRequest::serialize(CryptoNote::ISerializer& serializer) {
  serializer(index, "index");
}

void DeleteAddressRequest::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(address, "address");

  if (!r) {
    throw RequestSerializationError();
  }
}

void DeleteAddressResponse::serialize(CryptoNote::ISerializer& serializer) {
}

void CreateAddressResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(address, "address");
}

void GetAddressCountResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(count, "count");
}

void GetAddressResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(address, "address");
}

void GetActualBalanceRequest::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(address, "address");

  if (!r) {
    throw std::runtime_error("Required parameter is missing");
  }
}

void GetPendingBalanceRequest::serialize(CryptoNote::ISerializer& serializer) {
  serializer(address, "address");
}

void GetActualBalanceResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(actualBalance, "actual_balance");
}

void GetPendingBalanceResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(pendingBalance, "pending_balance");
}

void GetTransactionsCountResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(transactionsCount, "transactions_count");
}

void GetTransfersCountResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(transfersCount, "transfers_count");
}

void GetTransactionIdByTransferIdRequest::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(transferId, "transfer_id");

  if (!r) {
    throw RequestSerializationError();
  }
}

void GetTransactionIdByTransferIdResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(transactionid, "transaction_id");
}

void GetTransactionRequest::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(transactionId, "transaction_id");

  if (!r) {
    throw RequestSerializationError();
  }
}

void TransactionRpcInfo::serialize(CryptoNote::ISerializer& serializer) {
  serializer(firstTransferId, "first_transfer_id");
  serializer(transferCount, "transfer_count");
  serializer(totalAmount, "total_amount");
  serializer(fee, "fee");
  serializer(hash, "hash");
  serializer(blockHeight, "block_height");
  serializer(timestamp, "timestamp");
  serializer(extra, "extra");
  serializer(transfers, "transfers");
}

void GetTransactionResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(found, "found");

  if (found) {
    serializer(transactionInfo, "transaction_info");
  }
}

void ListTransactionsRequest::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(startingTransactionId, "starting_transaction_id");
  r &= serializer(maxTransactionCount, "max_transaction_count");

  if (!r) {
    throw std::runtime_error("Required parameter is missing");
  }
}

void ListTransactionsResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(transactions, "transactions");
}

void TransferRpcInfo::serialize(CryptoNote::ISerializer& serializer) {
  serializer(address, "address");
  serializer(amount, "amount");
}

void GetTransferRequest::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(transferId, "transfer_id");

  if (!r) {
    throw RequestSerializationError();
  }
}

void GetTransferResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(found, "found");
  if (found) {
    serializer(transferInfo, "transfer_info");
  }
}

void GetIncomingPaymentsRequest::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(payments, "payments");

  if (!r) {
    throw RequestSerializationError();
  }
}

void PaymentsById::serialize(CryptoNote::ISerializer& serializer) {
  serializer(id, "id");
  serializer(payments, "payments");
}

void GetIncomingPaymentsResponse::serialize(CryptoNote::ISerializer& serializer) {
  serializer(payments, "payments");
}

void PaymentDetails::serialize(CryptoNote::ISerializer& serializer) {
  serializer(txHash, "tx_hash");
  serializer(amount, "amount");
  serializer(blockHeight, "block_height");
  serializer(unlockTime, "unlock_time");
}

}
