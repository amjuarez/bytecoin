// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <limits>
#include <vector>
#include "crypto/hash.h"
#include "ITransaction.h"
#include "IObservable.h"
#include "IStreamSerializable.h"

namespace CryptoNote {

const uint64_t UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX = std::numeric_limits<uint64_t>::max();

struct TransactionInformation {
  // transaction info
  Hash transactionHash;
  PublicKey publicKey;
  uint64_t blockHeight;
  uint64_t timestamp;
  uint64_t unlockTime;
  uint64_t totalAmountIn;
  uint64_t totalAmountOut;
  std::vector<uint8_t> extra;
  Hash paymentId;
};


struct TransactionOutputInformation {
  // output info
  TransactionTypes::OutputType type;
  uint64_t amount;
  uint64_t globalOutputIndex;
  uint32_t outputInTransaction;

  // transaction info
  Hash transactionHash;
  PublicKey transactionPublicKey;

  union {
    PublicKey outputKey;         // Type: Key 
    uint32_t requiredSignatures; // Type: Multisignature
  };
};

struct TransactionSpentOutputInformation: public TransactionOutputInformation {
  uint64_t spendingBlockHeight;
  uint64_t timestamp;
  Hash spendingTransactionHash;
  KeyImage keyImage;  //!< \attention Used only for TransactionTypes::OutputType::Key
  uint32_t inputInTransaction;
};

class ITransfersContainer : public IStreamSerializable {
public:
  enum Flags : uint32_t {
    // state
    IncludeStateUnlocked = 0x01,
    IncludeStateLocked = 0x02,
    IncludeStateSoftLocked = 0x04,
    // output type
    IncludeTypeKey = 0x100,
    IncludeTypeMultisignature = 0x200,
    // combinations
    IncludeStateAll = 0xff,
    IncludeTypeAll = 0xff00,

    IncludeKeyUnlocked = IncludeTypeKey | IncludeStateUnlocked,
    IncludeKeyNotUnlocked = IncludeTypeKey | IncludeStateLocked | IncludeStateSoftLocked,

    IncludeAllLocked = IncludeTypeAll | IncludeStateLocked | IncludeStateSoftLocked,
    IncludeAllUnlocked = IncludeTypeAll | IncludeStateUnlocked,
    IncludeAll = IncludeTypeAll | IncludeStateAll,

    IncludeDefault = IncludeKeyUnlocked
  };

  virtual size_t transfersCount() = 0;
  virtual size_t transactionsCount() = 0;
  virtual uint64_t balance(uint32_t flags = IncludeDefault) = 0;
  virtual void getOutputs(std::vector<TransactionOutputInformation>& transfers, uint32_t flags = IncludeDefault) = 0;
  virtual bool getTransactionInformation(const Hash& transactionHash, TransactionInformation& info, int64_t& txBalance) = 0;
  virtual std::vector<TransactionOutputInformation> getTransactionOutputs(const Hash& transactionHash, uint32_t flags = IncludeDefault) = 0;
  virtual void getUnconfirmedTransactions(std::vector<crypto::hash>& transactions) = 0;
  virtual std::vector<TransactionSpentOutputInformation> getSpentOutputs() = 0;
};

}
