// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletSerialization.h"
#include "WalletUnconfirmedTransactions.h"
#include "IWallet.h"

#include "cryptonote_core/cryptonote_serialization.h"
#include "serialization/ISerializer.h"
#include "serialization/SerializationOverloads.h"

namespace CryptoNote {

void serialize(UnconfirmedTransferDetails& utd, const std::string& name, cryptonote::ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(utd.tx, "transaction");
  serializer(utd.amount, "amount");
  serializer(utd.outsAmount, "outs_amount");
  uint64_t time = static_cast<uint64_t>(utd.sentTime);
  serializer(time, "sent_time");
  utd.sentTime = static_cast<time_t>(time);
  uint64_t txId = static_cast<uint64_t>(utd.transactionId);
  serializer(txId, "transaction_id");
  utd.transactionId = static_cast<size_t>(txId);
  serializer.endObject();
}

void serialize(TransactionInfo& txi, const std::string& name, cryptonote::ISerializer& serializer) {
  serializer.beginObject(name);

  uint64_t trId = static_cast<uint64_t>(txi.firstTransferId);
  serializer(trId, "first_transfer_id");
  txi.firstTransferId = static_cast<size_t>(trId);

  uint64_t trCount = static_cast<uint64_t>(txi.transferCount);
  serializer(trCount, "transfer_count");
  txi.transferCount = static_cast<size_t>(trCount);

  serializer(txi.totalAmount, "total_amount");

  serializer(txi.fee, "fee");
  serializer(txi.hash, "hash");
  serializer(txi.isCoinbase, "is_coinbase");
  serializer(txi.blockHeight, "block_height");
  serializer(txi.timestamp, "timestamp");
  serializer(txi.unlockTime, "unlock_time");
  serializer(txi.extra, "extra");
  serializer.endObject();
}

void serialize(Transfer& tr, const std::string& name, cryptonote::ISerializer& serializer) {
  serializer.beginObject(name);
  serializer(tr.address, "address");
  serializer(tr.amount, "amount");
  serializer.endObject();
}

} //namespace CryptoNote
