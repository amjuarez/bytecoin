// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>

#include "INode.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "TestBlockchainGenerator.h"

class INodeDummyStub : public CryptoNote::INode
{
public:
  virtual bool addObserver(CryptoNote::INodeObserver* observer) { return true; };
  virtual bool removeObserver(CryptoNote::INodeObserver* observer) { return true; };

  virtual void init(const CryptoNote::INode::Callback& callback) {callback(std::error_code());};
  virtual bool shutdown() { return true; };

  virtual size_t getPeerCount() const { return 0; };
  virtual uint64_t getLastLocalBlockHeight() const { return 0; };
  virtual uint64_t getLastKnownBlockHeight() const { return 0; };

  virtual void getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback) {callback(std::error_code());};

  virtual void relayTransaction(const cryptonote::transaction& transaction, const Callback& callback) {callback(std::error_code());};
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) {callback(std::error_code());};
  virtual void getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback) { callback(std::error_code()); };
};

class INodeTrivialRefreshStub : public INodeDummyStub
{
public:
  INodeTrivialRefreshStub(TestBlockchainGenerator& generator) : m_lastHeight(1), m_blockchainGenerator(generator), m_nextTxError(false) {};

  virtual uint64_t getLastLocalBlockHeight() const { return m_blockchainGenerator.getBlockchain().size() - 1; };
  virtual uint64_t getLastKnownBlockHeight() const { return m_blockchainGenerator.getBlockchain().size() - 1; };

  virtual void getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback);

  virtual void relayTransaction(const cryptonote::transaction& transaction, const Callback& callback);
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback);
  virtual void getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback);

  virtual void startAlternativeChain(uint64_t height);
  virtual void setNextTransactionError();

private:
  void doGetNewBlocks(std::list<crypto::hash> knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback);
  void doGetTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback);
  void doRelayTransaction(const cryptonote::transaction& transaction, const Callback& callback);
  void doGetRandomOutsByAmounts(std::vector<uint64_t> amounts, uint64_t outsCount, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback);

  uint64_t m_lastHeight;
  TestBlockchainGenerator& m_blockchainGenerator;
  bool m_nextTxError;
};
