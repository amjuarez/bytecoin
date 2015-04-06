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

    if (type == TransactionTypes::OutputType::Key)
      s(outputKey, "");
    else if (type == TransactionTypes::OutputType::Multisignature)
      s(requiredSignatures, "");
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

  bool addTransaction(const BlockInfo& block, const ITransactionReader& tx, const std::vector<TransactionOutputInformationIn>& transfers);
  bool deleteUnconfirmedTransaction(const Hash& transactionHash);
  bool markTransactionConfirmed(const BlockInfo& block, const Hash& transactionHash, const std::vector<uint64_t>& globalIndices);

  std::vector<Hash> detach(uint64_t height);
  bool advanceHeight(uint64_t height);

  // ITransfersContainer
  virtual size_t transfersCount() override;
  virtual size_t transactionsCount() override;
  virtual uint64_t balance(uint32_t flags) override;
  virtual void getOutputs(std::vector<TransactionOutputInformation>& transfers, uint32_t flags) override;
  virtual bool getTransactionInformation(const Hash& transactionHash, TransactionInformation& info, int64_t& txBalance) override;
  virtual std::vector<TransactionOutputInformation> getTransactionOutputs(const Hash& transactionHash, uint32_t flags) override;
  virtual void getUnconfirmedTransactions(std::vector<crypto::hash>& transactions) override;
  virtual std::vector<TransactionSpentOutputInformation> getSpentOutputs() override;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

private:
  struct ContainingTransactionIndex { };
  struct SpendingTransactionIndex { };
  struct SpentOutputDescriptorIndex { };

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
      >
    >
  > SpentTransfersMultiIndex;

private:
  void addTransaction(const BlockInfo& block, const ITransactionReader& tx);
  bool addTransactionOutputs(const BlockInfo& block, const ITransactionReader& tx,
                             const std::vector<TransactionOutputInformationIn>& transfers);
  bool addTransactionInputs(const BlockInfo& block, const ITransactionReader& tx);
  void deleteTransactionTransfers(const Hash& transactionHash);
  bool isSpendTimeUnlocked(uint64_t unlockTime) const;
  bool isIncluded(const TransactionOutputInformationEx& info, uint32_t flags) const;
  static bool isIncluded(TransactionTypes::OutputType type, uint32_t state, uint32_t flags);
  void updateTransfersVisibility(const KeyImage& keyImage);

  void copyToSpent(const BlockInfo& block, const ITransactionReader& tx, size_t inputIndex, const TransactionOutputInformationEx& output);

private:
  TransactionMultiIndex m_transactions;
  UnconfirmedTransfersMultiIndex m_unconfirmedTransfers;
  AvailableTransfersMultiIndex m_availableTransfers;
  SpentTransfersMultiIndex m_spentTransfers;
  //std::unordered_map<KeyImage, KeyOutputInfo, boost::hash<KeyImage>> m_keyImages;

  uint64_t m_currentHeight; // current height is needed to check if a transfer is unlocked
  size_t m_transactionSpendableAge;
  const cryptonote::Currency& m_currency;
  std::mutex m_mutex;
};

}
