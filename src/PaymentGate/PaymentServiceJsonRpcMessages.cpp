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

void Reset::Request::serialize(CryptoNote::ISerializer& serializer) {
  serializer(viewSecretKey, "viewSecretKey");
}

void Reset::Response::serialize(CryptoNote::ISerializer& serializer) {
}

void GetViewKey::Request::serialize(CryptoNote::ISerializer& serializer) {
}

void GetViewKey::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(viewSecretKey, "viewSecretKey");
}

void GetStatus::Request::serialize(CryptoNote::ISerializer& serializer) {
}

void GetStatus::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(blockCount, "blockCount");
  serializer(knownBlockCount, "knownBlockCount");
  serializer(lastBlockHash, "lastBlockHash");
  serializer(peerCount, "peerCount");
}

void GetAddresses::Request::serialize(CryptoNote::ISerializer& serializer) {
}

void GetAddresses::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(addresses, "addresses");
}

void CreateAddress::Request::serialize(CryptoNote::ISerializer& serializer) {
  bool hasSecretKey = serializer(spendSecretKey, "spendSecretKey");
  bool hasPublicKey = serializer(spendPublicKey, "spendPublicKey");

  if (hasSecretKey && hasPublicKey) {
    //TODO: replace it with error codes
    throw RequestSerializationError();
  }
}

void CreateAddress::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(address, "address");
}

void DeleteAddress::Request::serialize(CryptoNote::ISerializer& serializer) {
  if (!serializer(address, "address")) {
    throw RequestSerializationError();
  }
}

void DeleteAddress::Response::serialize(CryptoNote::ISerializer& serializer) {
}

void GetSpendKeys::Request::serialize(CryptoNote::ISerializer& serializer) {
  if (!serializer(address, "address")) {
    throw RequestSerializationError();
  }
}

void GetSpendKeys::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(spendSecretKey, "spendSecretKey");
  serializer(spendPublicKey, "spendPublicKey");
}

void GetBalance::Request::serialize(CryptoNote::ISerializer& serializer) {
  serializer(address, "address");
}

void GetBalance::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(availableBalance, "availableBalance");
  serializer(lockedAmount, "lockedAmount");
}

void GetBlockHashes::Request::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(firstBlockIndex, "firstBlockIndex");
  r &= serializer(blockCount, "blockCount");

  if (!r) {
    throw RequestSerializationError();
  }
}

void GetBlockHashes::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(blockHashes, "blockHashes");
}

void TransactionHashesInBlockRpcInfo::serialize(CryptoNote::ISerializer& serializer) {
  serializer(blockHash, "blockHash");
  serializer(transactionHashes, "transactionHashes");
}

void GetTransactionHashes::Request::serialize(CryptoNote::ISerializer& serializer) {
  serializer(addresses, "addresses");

  if (serializer(blockHash, "blockHash") == serializer(firstBlockIndex, "firstBlockIndex")) {
    throw RequestSerializationError();
  }

  if (!serializer(blockCount, "blockCount")) {
    throw RequestSerializationError();
  }

  serializer(paymentId, "paymentId");
}

void GetTransactionHashes::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(items, "items");
}

void TransferRpcInfo::serialize(CryptoNote::ISerializer& serializer) {
  serializer(type, "type");
  serializer(address, "address");
  serializer(amount, "amount");
}

void TransactionRpcInfo::serialize(CryptoNote::ISerializer& serializer) {
  serializer(state, "state");
  serializer(transactionHash, "transactionHash");
  serializer(blockIndex, "blockIndex");
  serializer(timestamp, "timestamp");
  serializer(isBase, "isBase");
  serializer(unlockTime, "unlockTime");
  serializer(amount, "amount");
  serializer(fee, "fee");
  serializer(transfers, "transfers");
  serializer(extra, "extra");
  serializer(paymentId, "paymentId");
}

void GetTransaction::Request::serialize(CryptoNote::ISerializer& serializer) {
  if (!serializer(transactionHash, "transactionHash")) {
    throw RequestSerializationError();
  }
}

void GetTransaction::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(transaction, "transaction");
}

void TransactionsInBlockRpcInfo::serialize(CryptoNote::ISerializer& serializer) {
  serializer(blockHash, "blockHash");
  serializer(transactions, "transactions");
}

void GetTransactions::Request::serialize(CryptoNote::ISerializer& serializer) {
  serializer(addresses, "addresses");

  if (serializer(blockHash, "blockHash") == serializer(firstBlockIndex, "firstBlockIndex")) {
    throw RequestSerializationError();
  }

  if (!serializer(blockCount, "blockCount")) {
    throw RequestSerializationError();
  }

  serializer(paymentId, "paymentId");
}

void GetTransactions::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(items, "items");
}

void GetUnconfirmedTransactionHashes::Request::serialize(CryptoNote::ISerializer& serializer) {
  serializer(addresses, "addresses");
}

void GetUnconfirmedTransactionHashes::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(transactionHashes, "transactionHashes");
}

void WalletRpcOrder::serialize(CryptoNote::ISerializer& serializer) {
  bool r = serializer(address, "address");
  r &= serializer(amount, "amount");

  if (!r) {
    throw RequestSerializationError();
  }
}

void SendTransaction::Request::serialize(CryptoNote::ISerializer& serializer) {
  serializer(sourceAddresses, "addresses");

  if (!serializer(transfers, "transfers")) {
    throw RequestSerializationError();
  }

  serializer(changeAddress, "changeAddress");

  if (!serializer(fee, "fee")) {
    throw RequestSerializationError();
  }

  if (!serializer(anonymity, "anonymity")) {
    throw RequestSerializationError();
  }

  bool hasExtra = serializer(extra, "extra");
  bool hasPaymentId = serializer(paymentId, "paymentId");

  if (hasExtra && hasPaymentId) {
    throw RequestSerializationError();
  }

  serializer(unlockTime, "unlockTime");
}

void SendTransaction::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(transactionHash, "transactionHash");
}

void CreateDelayedTransaction::Request::serialize(CryptoNote::ISerializer& serializer) {
  serializer(addresses, "addresses");

  if (!serializer(transfers, "transfers")) {
    throw RequestSerializationError();
  }

  serializer(changeAddress, "changeAddress");

  if (!serializer(fee, "fee")) {
    throw RequestSerializationError();
  }

  if (!serializer(anonymity, "anonymity")) {
    throw RequestSerializationError();
  }

  bool hasExtra = serializer(extra, "extra");
  bool hasPaymentId = serializer(paymentId, "paymentId");

  if (hasExtra && hasPaymentId) {
    throw RequestSerializationError();
  }

  serializer(unlockTime, "unlockTime");
}

void CreateDelayedTransaction::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(transactionHash, "transactionHash");
}

void GetDelayedTransactionHashes::Request::serialize(CryptoNote::ISerializer& serializer) {
}

void GetDelayedTransactionHashes::Response::serialize(CryptoNote::ISerializer& serializer) {
  serializer(transactionHashes, "transactionHashes");
}

void DeleteDelayedTransaction::Request::serialize(CryptoNote::ISerializer& serializer) {
  if (!serializer(transactionHash, "transactionHash")) {
    throw RequestSerializationError();
  }
}

void DeleteDelayedTransaction::Response::serialize(CryptoNote::ISerializer& serializer) {
}

void SendDelayedTransaction::Request::serialize(CryptoNote::ISerializer& serializer) {
  if (!serializer(transactionHash, "transactionHash")) {
    throw RequestSerializationError();
  }
}

void SendDelayedTransaction::Response::serialize(CryptoNote::ISerializer& serializer) {
}

}
