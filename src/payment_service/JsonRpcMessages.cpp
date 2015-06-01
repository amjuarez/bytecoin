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

#include "JsonRpcMessages.h"
#include "serialization/SerializationOverloads.h"

namespace PaymentService {

namespace {

void throwIfRequiredParamsMissing(CryptoNote::ISerializer& serializer, const std::vector<const char*>& names) {
  bool r = true;
  for (const auto name: names) {
    r &= serializer.hasObject(name);
  }

  if (!r) {
    throw RequestSerializationError();
  }
}

void throwIfRequiredParamsMissing(CryptoNote::ISerializer& serializer, const char* name) {
  throwIfRequiredParamsMissing(serializer, std::vector<const char *>{name});
}

}

void TransferDestination::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  throwIfRequiredParamsMissing(serializer, {"amount", "address"});
  serializer(amount, "amount");
  serializer(address, "address");
  serializer.endObject();
}

void SendTransactionRequest::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  throwIfRequiredParamsMissing(serializer, {"destinations", "fee", "mixin"});

  serializer.beginObject(name);

  size_t size = destinations.size();
  serializer.beginArray(size, "destinations");
  destinations.resize(size);

  auto it = destinations.begin();
  for (size_t i = 0; i < size; ++i, ++it) {
    it->serialize(serializer, "");
  }
  serializer.endArray();

  serializer(fee, "fee");
  serializer(mixin, "mixin");

  if (serializer.hasObject("unlock_time")) {
    serializer(unlockTime, "unlock_time");
  }

  if (serializer.hasObject("payment_id")) {
    serializer(paymentId, "payment_id");
  }

  serializer.endObject();
}

void SendTransactionResponse::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  serializer(transactionId, "transaction_id");
  serializer.endObject();
}

void GetAddressResponse::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  serializer(address, "address");
  serializer.endObject();
}

void GetActualBalanceResponse::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  serializer(actualBalance, "actual_balance");
  serializer.endObject();
}

void GetPendingBalanceResponse::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  serializer(pendingBalance, "pending_balance");
  serializer.endObject();
}

void GetTransactionsCountResponse::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  serializer(transactionsCount, "transactions_count");
  serializer.endObject();
}

void GetTransfersCountResponse::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  serializer(transfersCount, "transfers_count");
  serializer.endObject();
}

void GetTransactionIdByTransferIdRequest::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  throwIfRequiredParamsMissing(serializer, "transfer_id");

  serializer.beginObject(name);
  serializer(transferId, "transfer_id");
  serializer.endObject();
}

void GetTransactionIdByTransferIdResponse::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  serializer(transactionid, "transaction_id");
  serializer.endObject();
}

void GetTransactionRequest::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  throwIfRequiredParamsMissing(serializer, "transaction_id");

  serializer.beginObject(name);
  serializer(transactionId, "transaction_id");
  serializer.endObject();
}

void TransactionRpcInfo::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);

  serializer(firstTransferId, "first_transfer_id");
  serializer(transferCount, "transfer_count");
  serializer(totalAmount, "total_amount");
  serializer(fee, "fee");
  serializer(hash, "hash");
  serializer(isCoinbase, "is_coin_base");
  serializer(blockHeight, "block_height");
  serializer(timestamp, "timestamp");
  serializer(extra, "extra");

  serializer.endObject();
}

void GetTransactionResponse::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);

  serializer(found, "found");

  if (!found) {
    serializer.endObject();
    return;
  }

  transactionInfo.serialize(serializer, "transaction_info");

  serializer.endObject();
}

void TransferRpcInfo::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  serializer(address, "address");
  serializer(amount, "amount");
  serializer.endObject();
}

void GetTransferRequest::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  throwIfRequiredParamsMissing(serializer, "transfer_id");

  serializer.beginObject(name);
  serializer(transferId, "transfer_id");
  serializer.endObject();
}

void GetTransferResponse::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  serializer(found, "found");

  if (!found) {
    serializer.endObject();
    return;
  }

  transferInfo.serialize(serializer, "transfer_info");

  serializer.endObject();
}

void GetIncomingPaymentsRequest::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  throwIfRequiredParamsMissing(serializer, "payments");

  serializer.beginObject(name);
  serializer(payments, "payments");
  serializer.endObject();
}

void PaymentsById::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);

  serializer(id, "id");
  serializer(payments, "payments");

  serializer.endObject();
}

void GetIncomingPaymentsResponse::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);

  serializer(payments, "payments");

  serializer.endObject();
}

void PaymentDetails::serialize(CryptoNote::ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);
  serializer(txHash, "tx_hash");
  serializer(amount, "amount");
  serializer(blockHeight, "block_height");
  serializer(unlockTime, "unlock_time");
  serializer.endObject();
}

}
