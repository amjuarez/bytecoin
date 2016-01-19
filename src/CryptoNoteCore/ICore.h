// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <list>
#include <utility>
#include <vector>
#include <system_error>

#include <CryptoNote.h>
#include "CryptoNoteCore/Difficulty.h"

#include "CryptoNoteCore/MessageQueue.h"
#include "CryptoNoteCore/BlockchainMessages.h"

namespace CryptoNote {

struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request;
struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response;
struct NOTIFY_RESPONSE_GET_OBJECTS_request;
struct NOTIFY_REQUEST_GET_OBJECTS_request;

class Currency;
class IBlock;
class ICoreObserver;
struct Block;
struct block_verification_context;
struct BlockFullInfo;
struct BlockShortInfo;
struct core_stat_info;
struct i_cryptonote_protocol;
struct Transaction;
struct MultisignatureInput;
struct KeyInput;
struct TransactionPrefixInfo;
struct tx_verification_context;

class ICore {
public:
  virtual ~ICore() {}

  virtual bool addObserver(ICoreObserver* observer) = 0;
  virtual bool removeObserver(ICoreObserver* observer) = 0;

  virtual bool have_block(const Crypto::Hash& id) = 0;
  virtual std::vector<Crypto::Hash> buildSparseChain() = 0;
  virtual std::vector<Crypto::Hash> buildSparseChain(const Crypto::Hash& startBlockId) = 0;
  virtual bool get_stat_info(CryptoNote::core_stat_info& st_inf) = 0;
  virtual bool on_idle() = 0;
  virtual void pause_mining() = 0;
  virtual void update_block_template_and_resume_mining() = 0;
  virtual bool handle_incoming_block_blob(const CryptoNote::BinaryArray& block_blob, CryptoNote::block_verification_context& bvc, bool control_miner, bool relay_block) = 0;
  virtual bool handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS_request& arg, NOTIFY_RESPONSE_GET_OBJECTS_request& rsp) = 0; //Deprecated. Should be removed with CryptoNoteProtocolHandler.
  virtual void on_synchronized() = 0;
  virtual size_t addChain(const std::vector<const IBlock*>& chain) = 0;

  virtual void get_blockchain_top(uint32_t& height, Crypto::Hash& top_id) = 0;
  virtual std::vector<Crypto::Hash> findBlockchainSupplement(const std::vector<Crypto::Hash>& remoteBlockIds, size_t maxCount,
    uint32_t& totalBlockCount, uint32_t& startBlockIndex) = 0;
  virtual bool get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& res) = 0;
  virtual bool get_tx_outputs_gindexs(const Crypto::Hash& tx_id, std::vector<uint32_t>& indexs) = 0;
  virtual bool getOutByMSigGIndex(uint64_t amount, uint64_t gindex, MultisignatureOutput& out) = 0;
  virtual i_cryptonote_protocol* get_protocol() = 0;
  virtual bool handle_incoming_tx(const BinaryArray& tx_blob, tx_verification_context& tvc, bool keeped_by_block) = 0; //Deprecated. Should be removed with CryptoNoteProtocolHandler.
  virtual std::vector<Transaction> getPoolTransactions() = 0;
  virtual bool getPoolChanges(const Crypto::Hash& tailBlockId, const std::vector<Crypto::Hash>& knownTxsIds,
                              std::vector<Transaction>& addedTxs, std::vector<Crypto::Hash>& deletedTxsIds) = 0;
  virtual bool getPoolChangesLite(const Crypto::Hash& tailBlockId, const std::vector<Crypto::Hash>& knownTxsIds,
                              std::vector<TransactionPrefixInfo>& addedTxs, std::vector<Crypto::Hash>& deletedTxsIds) = 0;
  virtual void getPoolChanges(const std::vector<Crypto::Hash>& knownTxsIds, std::vector<Transaction>& addedTxs,
                              std::vector<Crypto::Hash>& deletedTxsIds) = 0;
  virtual bool queryBlocks(const std::vector<Crypto::Hash>& block_ids, uint64_t timestamp,
    uint32_t& start_height, uint32_t& current_height, uint32_t& full_offset, std::vector<BlockFullInfo>& entries) = 0;
  virtual bool queryBlocksLite(const std::vector<Crypto::Hash>& block_ids, uint64_t timestamp,
    uint32_t& start_height, uint32_t& current_height, uint32_t& full_offset, std::vector<BlockShortInfo>& entries) = 0;

  virtual Crypto::Hash getBlockIdByHeight(uint32_t height) = 0;
  virtual bool getBlockByHash(const Crypto::Hash &h, Block &blk) = 0;
  virtual bool getBlockHeight(const Crypto::Hash& blockId, uint32_t& blockHeight) = 0;
  virtual void getTransactions(const std::vector<Crypto::Hash>& txs_ids, std::list<Transaction>& txs, std::list<Crypto::Hash>& missed_txs, bool checkTxPool = false) = 0;
  virtual bool getBackwardBlocksSizes(uint32_t fromHeight, std::vector<size_t>& sizes, size_t count) = 0;
  virtual bool getBlockSize(const Crypto::Hash& hash, size_t& size) = 0;
  virtual bool getAlreadyGeneratedCoins(const Crypto::Hash& hash, uint64_t& generatedCoins) = 0;
  virtual bool getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee,
                              uint64_t& reward, int64_t& emissionChange) = 0;
  virtual bool scanOutputkeysForIndices(const KeyInput& txInToKey, std::list<std::pair<Crypto::Hash, size_t>>& outputReferences) = 0;
  virtual bool getBlockDifficulty(uint32_t height, difficulty_type& difficulty) = 0;
  virtual bool getBlockContainingTx(const Crypto::Hash& txId, Crypto::Hash& blockId, uint32_t& blockHeight) = 0;
  virtual bool getMultisigOutputReference(const MultisignatureInput& txInMultisig, std::pair<Crypto::Hash, size_t>& outputReference) = 0;

  virtual bool getGeneratedTransactionsNumber(uint32_t height, uint64_t& generatedTransactions) = 0;
  virtual bool getOrphanBlocksByHeight(uint32_t height, std::vector<Block>& blocks) = 0;
  virtual bool getBlocksByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<Block>& blocks, uint32_t& blocksNumberWithinTimestamps) = 0;
  virtual bool getPoolTransactionsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<Transaction>& transactions, uint64_t& transactionsNumberWithinTimestamps) = 0;
  virtual bool getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<Transaction>& transactions) = 0;

  virtual std::unique_ptr<IBlock> getBlock(const Crypto::Hash& blocksId) = 0;
  virtual bool handleIncomingTransaction(const Transaction& tx, const Crypto::Hash& txHash, size_t blobSize, tx_verification_context& tvc, bool keptByBlock) = 0;
  virtual std::error_code executeLocked(const std::function<std::error_code()>& func) = 0;

  virtual bool addMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) = 0;
  virtual bool removeMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) = 0;
};

} //namespace CryptoNote
