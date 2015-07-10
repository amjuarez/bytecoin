// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <unordered_map>
#include <mutex>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include "crypto/crypto.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/Currency.h"
#include "serialization/ISerializer.h"
#include "serialization/SerializationOverloads.h"

#include "ITransaction.h"
#include "ITransfersContainer.h"
#include "SerializationHelpers.h"

namespace CryptoNote {

struct TransactionOutputInformationIn;

struct TransactionOutputId {
  Hash transactionHash;
  uint32_t outputInTransaction;

  size_t hash() const;
  bool operator==(const TransactionOutputId& rhs) const {
    if (transactionHash != rhs.transactionHash) {
      return false;
    }

    if (outputInTransaction != rhs.outputInTransaction) {
      return false;
    }

    return true;
  }

  void serialize(cryptonote::ISerializer& s, const std::string& name) {
    s(transactionHash, "transactionHash");
    s(outputInTransaction, "outputInTransaction");
  }
};

struct TransactionOutputIdHasher {
  size_t operator() (const TransactionOutputId& outputId) const {
    return outputId.hash();
  }
};

class SpentOutputDescriptor {
public:
  SpentOutputDescriptor();
  SpentOutputDescriptor(const TransactionOutputInformationIn& transactionInfo);
  SpentOutputDescriptor(const KeyImage* keyImage);
  SpentOutputDescriptor(uint64_t amount, uint64_t globalOutputIndex);

  void assign(const KeyImage* keyImage);
  void assign(uint64_t amount, uint64_t globalOutputIndex);

  bool isValid() const;

  bool operator==(const SpentOutputDescriptor& other) const;
  size_t hash() const;

private:
  TransactionTypes::OutputType m_type;
  union {
    const KeyImage* m_keyImage;
struct {
      uint64_t m_amount;
      uint64_t m_globalOutputIndex;
    };
  };
};

struct SpentOutputDescriptorHasher {
  size_t operator()(const SpentOutputDescriptor& descriptor) const {
    return descriptor.hash();
  }
};

struct TransactionOutputInformationIn : public TransactionOutputInformation {
  KeyImage keyImage;  //!< \attention Used only for TransactionTypes::OutputType::Key
};

struct TransactionOutputInformationEx : public TransactionOutputInformationIn {
  uint64_t unlockTime;
  uint64_t blockHeight;
  uint32_t transactionIndex;
  bool visible;

  SpentOutputDescriptor getSpentOutputDescriptor() const { return SpentOutputDescriptor(*this); }
  const Hash& getTransactionHash() const { return transactionHash; }

  TransactionOutputId getTransactionOutputId() const { return TransactionOutputId {transactionHash, outputInTransaction}; }

  void serialize(cryptonote::ISerializer& s, const std::string& name) {
    s(reinterpret_cast<uint8_t&>(type), "type");
    s(amount, "");
    s(globalOutputIndex, "");
    s(outputInTransaction, "");
    s(transactionPublicKey, "");
    s(keyImage, "");
    s(unlockTime, "");
    s(blockHeight, "");
    s(transactionIndex, "");
    s(transactionHash, "");
    s(visible, "");

    if (type == TransactionTypes::OutputType::Key) {
      s(outputKey, "");
    } else if (type == TransactionTypes::OutputType::Multisignature) {
      s(requiredSignatures, "");
      s(term, "");
    }
  }

};

struct BlockInfo {
  uint64_t height;
  uint64_t timestamp;
  uint32_t transactionIndex;

  void serialize(cryptonote::ISerializer& s, const std::string& name) {
    s(height, "height");
    s(timestamp, "timestamp");
    s(transactionIndex, "transactionIndex");
  }
};

struct SpentTransactionOutput : TransactionOutputInformationEx {
  BlockInfo spendingBlock;
  Hash spendingTransactionHash;
  uint32_t inputInTransaction;

  const Hash& getSpendingTransactionHash() const {
    return spendingTransactionHash;
  }

  void serialize(cryptonote::ISerializer& s, const std::string& name) {
    TransactionOutputInformationEx::serialize(s, name);
    s(spendingBlock, "spendingBlock");
    s(spendingTransactionHash, "spendingTransactionHash");
    s(inputInTransaction, "inputInTransaction");
  }
};

struct TransferUnlockJob {
  uint64_t unlockHeight;
  TransactionOutputId transactionOutputId;

  Hash getTransactionHash() const { return transactionOutputId.transactionHash; }

  void serialize(cryptonote::ISerializer& s, const std::string& name) {
    s(unlockHeight, "unlockHeight");
    s(transactionOutputId, "transactionOutputId");
  }
};

enum class KeyImageState {
  Unconfirmed,
  Confirmed,
  Spent
};

struct KeyOutputInfo {
  KeyImageState state;
  size_t count;
};

class TransfersContainer : public ITransfersContainer {

public:

  TransfersContainer(const cryptonote::Currency& currency, size_t transactionSpendableAge);

  bool addTransaction(const BlockInfo& block, const ITransactionReader& tx,
                      const std::vector<TransactionOutputInformationIn>& transfers,
                      std::vector<std::string>&& messages, std::vector<TransactionOutputInformation>* unlockingTransfers = nullptr);
  bool deleteUnconfirmedTransaction(const Hash& transactionHash);
  bool markTransactionConfirmed(const BlockInfo& block, const Hash& transactionHash, const std::vector<uint64_t>& globalIndices);

  void detach(uint64_t height, std::vector<Hash>& deletedTransactions, std::vector<TransactionOutputInformation>& lockedTransfers);
  //returns outputs that are being unlocked
  std::vector<TransactionOutputInformation> advanceHeight(uint64_t height);

  // ITransfersContainer
  virtual size_t transfersCount() override;
  virtual size_t transactionsCount() override;
  virtual uint64_t balance(uint32_t flags) override;
  virtual void getOutputs(std::vector<TransactionOutputInformation>& transfers, uint32_t flags) override;
  virtual bool getTransactionInformation(const Hash& transactionHash, TransactionInformation& info, int64_t& txBalance) override;
  virtual std::vector<TransactionOutputInformation> getTransactionOutputs(const Hash& transactionHash, uint32_t flags) override;
  //only type flags are feasible for this function
  virtual std::vector<TransactionOutputInformation> getTransactionInputs(const Hash& transactionHash, uint32_t flags) const override;
  virtual void getUnconfirmedTransactions(std::vector<crypto::hash>& transactions) override;
  virtual std::vector<TransactionSpentOutputInformation> getSpentOutputs() override;
  virtual bool getTransfer(const Hash& transactionHash, uint32_t outputInTransaction, TransactionOutputInformation& transfer, TransferState& transferState) const override;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

private:
  struct ContainingTransactionIndex { };
  struct SpendingTransactionIndex { };
  struct SpentOutputDescriptorIndex { };
  struct TransferUnlockHeightIndex { };
  struct TransactionOutputIdIndex { };

  typedef boost::multi_index_container<
    TransactionInformation,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_unique<BOOST_MULTI_INDEX_MEMBER(TransactionInformation, Hash, transactionHash)>,
      boost::multi_index::ordered_non_unique<BOOST_MULTI_INDEX_MEMBER(TransactionInformation, uint64_t, blockHeight)>
    >
  > TransactionMultiIndex;

  typedef boost::multi_index_container<
    TransactionOutputInformationEx,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<SpentOutputDescriptorIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          SpentOutputDescriptor,
          &TransactionOutputInformationEx::getSpentOutputDescriptor>,
        SpentOutputDescriptorHasher
      >,
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<ContainingTransactionIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          const Hash&,
          &TransactionOutputInformationEx::getTransactionHash>
      >,
      boost::multi_index::hashed_unique <
        boost::multi_index::tag<TransactionOutputIdIndex>,
        boost::multi_index::const_mem_fun <
          TransactionOutputInformationEx,
          TransactionOutputId,
          &TransactionOutputInformationEx::getTransactionOutputId>,
        TransactionOutputIdHasher
      >
    >
  > UnconfirmedTransfersMultiIndex;

  typedef boost::multi_index_container<
    TransactionOutputInformationEx,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<SpentOutputDescriptorIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          SpentOutputDescriptor,
          &TransactionOutputInformationEx::getSpentOutputDescriptor>,
        SpentOutputDescriptorHasher
      >,
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<ContainingTransactionIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          const Hash&,
          &TransactionOutputInformationEx::getTransactionHash>
      >,
      boost::multi_index::hashed_unique <
        boost::multi_index::tag<TransactionOutputIdIndex>,
        boost::multi_index::const_mem_fun <
          TransactionOutputInformationEx,
          TransactionOutputId,
          &TransactionOutputInformationEx::getTransactionOutputId>,
        TransactionOutputIdHasher
      >
    >
  > AvailableTransfersMultiIndex;

  typedef boost::multi_index_container<
    SpentTransactionOutput,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_unique<
        boost::multi_index::tag<SpentOutputDescriptorIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          SpentOutputDescriptor,
          &TransactionOutputInformationEx::getSpentOutputDescriptor>,
        SpentOutputDescriptorHasher
      >,
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<ContainingTransactionIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          const Hash&,
          &SpentTransactionOutput::getTransactionHash>
      >,
      boost::multi_index::hashed_non_unique <
        boost::multi_index::tag<SpendingTransactionIndex>,
        boost::multi_index::const_mem_fun <
          SpentTransactionOutput,
          const Hash&,
          &SpentTransactionOutput::getSpendingTransactionHash>
      >,
      boost::multi_index::hashed_unique <
        boost::multi_index::tag<TransactionOutputIdIndex>,
        boost::multi_index::const_mem_fun <
          TransactionOutputInformationEx,
          TransactionOutputId,
          &TransactionOutputInformationEx::getTransactionOutputId>,
        TransactionOutputIdHasher
      >
    >
  > SpentTransfersMultiIndex;

  typedef boost::multi_index_container<
    TransferUnlockJob,
    boost::multi_index::indexed_by<
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<TransferUnlockHeightIndex>,
        BOOST_MULTI_INDEX_MEMBER(TransferUnlockJob, uint64_t, unlockHeight)
      >,
      boost::multi_index::hashed_unique<
        boost::multi_index::tag<TransactionOutputIdIndex>,
        BOOST_MULTI_INDEX_MEMBER(TransferUnlockJob, TransactionOutputId, transactionOutputId),
        TransactionOutputIdHasher
      >
    >
  > TransfersUnlockMultiIndex;

private:
  void addTransaction(const BlockInfo& block, const ITransactionReader& tx, std::vector<std::string>&& messages);
  bool addTransactionOutputs(const BlockInfo& block, const ITransactionReader& tx,
                             const std::vector<TransactionOutputInformationIn>& transfers);
  bool addTransactionInputs(const BlockInfo& block, const ITransactionReader& tx);
  void deleteTransactionTransfers(const Hash& transactionHash);
  bool isSpendTimeUnlocked(const TransactionOutputInformationEx& info) const;
  bool isIncluded(const TransactionOutputInformationEx& info, uint32_t flags) const;
  static bool isIncluded(const TransactionOutputInformationEx& output, uint32_t state, uint32_t flags);
  void updateTransfersVisibility(const KeyImage& keyImage);
  void addUnlockJob(const TransactionOutputInformationEx& output);
  void deleteUnlockJob(const TransactionOutputInformationEx& output);
  std::vector<TransactionOutputInformation> getUnlockingTransfers(uint64_t prevHeight, uint64_t currentHeight);
  void getLockingTransfers(uint64_t prevHeight, uint64_t currentHeight,
    const std::vector<Hash>& deletedTransactions, std::vector<TransactionOutputInformation>& lockingTransfers);
  TransactionOutputInformation getAvailableOutput(const TransactionOutputId& transactionOutputId) const;

  void copyToSpent(const BlockInfo& block, const ITransactionReader& tx, size_t inputIndex, const TransactionOutputInformationEx& output);

  void rebuildTransfersUnlockJobs(TransfersUnlockMultiIndex& transfersUnlockJobs, const AvailableTransfersMultiIndex& availableTransfers,
                                  const SpentTransfersMultiIndex& spentTransfers);
  std::vector<TransactionOutputInformation> doAdvanceHeight(uint64_t height);

private:
  TransactionMultiIndex m_transactions;
  UnconfirmedTransfersMultiIndex m_unconfirmedTransfers;
  AvailableTransfersMultiIndex m_availableTransfers;
  SpentTransfersMultiIndex m_spentTransfers;
  TransfersUnlockMultiIndex m_transfersUnlockJobs;
  //std::unordered_map<KeyImage, KeyOutputInfo, boost::hash<KeyImage>> m_keyImages;

  uint64_t m_currentHeight; // current height is needed to check if a transfer is unlocked
  size_t m_transactionSpendableAge;
  const cryptonote::Currency& m_currency;
  mutable std::mutex m_mutex;
};

}
