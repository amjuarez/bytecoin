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

#include "TestGenerator.h"

// epee
#include "misc_language.h"

#include "cryptonote_core/account.h"
#include "cryptonote_core/miner.h"

using namespace std;

using namespace epee;
using namespace cryptonote;


void test_generator::getBlockchain(std::vector<BlockInfo>& blockchain, const crypto::hash& head, size_t n) const {
  crypto::hash curr = head;
  while (null_hash != curr && blockchain.size() < n) {
    auto it = m_blocksInfo.find(curr);
    if (m_blocksInfo.end() == it) {
      throw std::runtime_error("block hash wasn't found");
    }

    blockchain.push_back(it->second);
    curr = it->second.prevId;
  }

  std::reverse(blockchain.begin(), blockchain.end());
}

void test_generator::getLastNBlockSizes(std::vector<size_t>& blockSizes, const crypto::hash& head, size_t n) const {
  std::vector<BlockInfo> blockchain;
  getBlockchain(blockchain, head, n);
  for (auto& bi : blockchain) {
    blockSizes.push_back(bi.blockSize);
  }
}

uint64_t test_generator::getAlreadyGeneratedCoins(const crypto::hash& blockId) const {
  auto it = m_blocksInfo.find(blockId);
  if (it == m_blocksInfo.end()) {
    throw std::runtime_error("block hash wasn't found");
  }

  return it->second.alreadyGeneratedCoins;
}

uint64_t test_generator::getAlreadyGeneratedCoins(const cryptonote::Block& blk) const {
  crypto::hash blkHash;
  get_block_hash(blk, blkHash);
  return getAlreadyGeneratedCoins(blkHash);
}

void test_generator::addBlock(const cryptonote::Block& blk, size_t tsxSize, uint64_t fee,
                              std::vector<size_t>& blockSizes, uint64_t alreadyGeneratedCoins) {
  const size_t blockSize = tsxSize + get_object_blobsize(blk.minerTx);
  int64_t emissionChange;
  uint64_t blockReward;
  bool penalizeFee = blk.majorVersion > BLOCK_MAJOR_VERSION_1;
  m_currency.getBlockReward(misc_utils::median(blockSizes), blockSize, alreadyGeneratedCoins, fee, penalizeFee,
    blockReward, emissionChange);
  m_blocksInfo[get_block_hash(blk)] = BlockInfo(blk.prevId, alreadyGeneratedCoins + emissionChange, blockSize);
}

bool test_generator::constructBlock(cryptonote::Block& blk, uint64_t height, const crypto::hash& prevId,
                                    const cryptonote::account_base& minerAcc, uint64_t timestamp, uint64_t alreadyGeneratedCoins,
                                    std::vector<size_t>& blockSizes, const std::list<cryptonote::Transaction>& txList) {
  blk.majorVersion = defaultMajorVersion;
  blk.minorVersion = defaultMinorVersion;
  blk.timestamp = timestamp;
  blk.prevId = prevId;

  blk.txHashes.reserve(txList.size());
  for (const Transaction &tx : txList) {
    crypto::hash tx_hash;
    get_transaction_hash(tx, tx_hash);
    blk.txHashes.push_back(tx_hash);
  }

  uint64_t totalFee = 0;
  size_t txsSize = 0;
  for (auto& tx : txList) {
    uint64_t fee = 0;
    bool r = get_tx_fee(tx, fee);
    CHECK_AND_ASSERT_MES(r, false, "wrong transaction passed to construct_block");
    totalFee += fee;
    txsSize += get_object_blobsize(tx);
  }

  blk.minerTx = AUTO_VAL_INIT(blk.minerTx);
  size_t targetBlockSize = txsSize + get_object_blobsize(blk.minerTx);
  while (true) {
    if (!m_currency.constructMinerTx(height, misc_utils::median(blockSizes), alreadyGeneratedCoins, targetBlockSize,
      totalFee, minerAcc.get_keys().m_account_address, blk.minerTx, blobdata(), 10)) {
      return false;
    }

    size_t actualBlockSize = txsSize + get_object_blobsize(blk.minerTx);
    if (targetBlockSize < actualBlockSize) {
      targetBlockSize = actualBlockSize;
    } else if (actualBlockSize < targetBlockSize) {
      size_t delta = targetBlockSize - actualBlockSize;
      blk.minerTx.extra.resize(blk.minerTx.extra.size() + delta, 0);
      actualBlockSize = txsSize + get_object_blobsize(blk.minerTx);
      if (actualBlockSize == targetBlockSize) {
        break;
      } else {
        CHECK_AND_ASSERT_MES(targetBlockSize < actualBlockSize, false, "Unexpected block size");
        delta = actualBlockSize - targetBlockSize;
        blk.minerTx.extra.resize(blk.minerTx.extra.size() - delta);
        actualBlockSize = txsSize + get_object_blobsize(blk.minerTx);
        if (actualBlockSize == targetBlockSize) {
          break;
        } else {
          CHECK_AND_ASSERT_MES(actualBlockSize < targetBlockSize, false, "Unexpected block size");
          blk.minerTx.extra.resize(blk.minerTx.extra.size() + delta, 0);
          targetBlockSize = txsSize + get_object_blobsize(blk.minerTx);
        }
      }
    } else {
      break;
    }
  }

  if (blk.majorVersion >= BLOCK_MAJOR_VERSION_2) {
    blk.parentBlock.majorVersion = BLOCK_MAJOR_VERSION_1;
    blk.parentBlock.minorVersion = BLOCK_MINOR_VERSION_0;
    blk.parentBlock.numberOfTransactions = 1;

    cryptonote::tx_extra_merge_mining_tag mmTag;
    mmTag.depth = 0;
    if (!cryptonote::get_aux_block_header_hash(blk, mmTag.merkle_root)) {
      return false;
    }

    blk.parentBlock.minerTx.extra.clear();
    if (!cryptonote::append_mm_tag_to_extra(blk.parentBlock.minerTx.extra, mmTag)) {
      return false;
    }
  }

  // Nonce search...
  blk.nonce = 0;
  crypto::cn_context context;
  while (!miner::find_nonce_for_given_block(context, blk, getTestDifficulty())) {
    blk.timestamp++;
  }

  addBlock(blk, txsSize, totalFee, blockSizes, alreadyGeneratedCoins);

  return true;
}

bool test_generator::constructBlock(cryptonote::Block& blk, const cryptonote::account_base& minerAcc, uint64_t timestamp) {
  std::vector<size_t> blockSizes;
  std::list<cryptonote::Transaction> txList;
  return constructBlock(blk, 0, null_hash, minerAcc, timestamp, 0, blockSizes, txList);
}

bool test_generator::constructBlock(cryptonote::Block& blk, const cryptonote::Block& blkPrev,
                                    const cryptonote::account_base& minerAcc,
                                    const std::list<cryptonote::Transaction>& txList/* = std::list<cryptonote::Transaction>()*/) {
  uint64_t height = boost::get<TransactionInputGenerate>(blkPrev.minerTx.vin.front()).height + 1;
  crypto::hash prevId = get_block_hash(blkPrev);
  // Keep difficulty unchanged
  uint64_t timestamp = blkPrev.timestamp + m_currency.difficultyTarget();
  uint64_t alreadyGeneratedCoins = getAlreadyGeneratedCoins(prevId);
  std::vector<size_t> blockSizes;
  getLastNBlockSizes(blockSizes, prevId, m_currency.rewardBlocksWindow());

  return constructBlock(blk, height, prevId, minerAcc, timestamp, alreadyGeneratedCoins, blockSizes, txList);
}

bool test_generator::constructBlockManually(Block& blk, const Block& prevBlock, const account_base& minerAcc,
                                            int actualParams/* = bf_none*/, uint8_t majorVer/* = 0*/,
                                            uint8_t minorVer/* = 0*/, uint64_t timestamp/* = 0*/,
                                            const crypto::hash& prevId/* = crypto::hash()*/, const difficulty_type& diffic/* = 1*/,
                                            const Transaction& minerTx/* = transaction()*/,
                                            const std::vector<crypto::hash>& txHashes/* = std::vector<crypto::hash>()*/,
                                            size_t txsSizes/* = 0*/, uint64_t fee/* = 0*/) {
  blk.majorVersion = actualParams & bf_major_ver ? majorVer  : defaultMajorVersion;
  blk.minorVersion = actualParams & bf_minor_ver ? minorVer  : defaultMinorVersion;
  blk.timestamp    = actualParams & bf_timestamp ? timestamp : prevBlock.timestamp + m_currency.difficultyTarget(); // Keep difficulty unchanged
  blk.prevId       = actualParams & bf_prev_id   ? prevId    : get_block_hash(prevBlock);
  blk.txHashes     = actualParams & bf_tx_hashes ? txHashes  : std::vector<crypto::hash>();

  size_t height = get_block_height(prevBlock) + 1;
  uint64_t alreadyGeneratedCoins = getAlreadyGeneratedCoins(prevBlock);
  std::vector<size_t> blockSizes;
  getLastNBlockSizes(blockSizes, get_block_hash(prevBlock), m_currency.rewardBlocksWindow());
  if (actualParams & bf_miner_tx) {
    blk.minerTx = minerTx;
  } else {
    size_t currentBlockSize = txsSizes + get_object_blobsize(blk.minerTx);
    // TODO: This will work, until size of constructed block is less then m_currency.blockGrantedFullRewardZone()
    if (!m_currency.constructMinerTx(height, misc_utils::median(blockSizes), alreadyGeneratedCoins, currentBlockSize, 0,
      minerAcc.get_keys().m_account_address, blk.minerTx, blobdata(), 1, blk.majorVersion > BLOCK_MAJOR_VERSION_1)) {
        return false;
    }
  }

  if (blk.majorVersion >= BLOCK_MAJOR_VERSION_2) {
    blk.parentBlock.majorVersion = BLOCK_MAJOR_VERSION_1;
    blk.parentBlock.minorVersion = BLOCK_MINOR_VERSION_0;
    blk.parentBlock.numberOfTransactions = 1;

    cryptonote::tx_extra_merge_mining_tag mmTag;
    mmTag.depth = 0;
    if (!cryptonote::get_aux_block_header_hash(blk, mmTag.merkle_root)) {
      return false;
    }

    blk.parentBlock.minerTx.extra.clear();
    if (!cryptonote::append_mm_tag_to_extra(blk.parentBlock.minerTx.extra, mmTag)) {
      return false;
    }
  }

  difficulty_type aDiffic = actualParams & bf_diffic ? diffic : getTestDifficulty();
  if (1 < aDiffic) {
    fillNonce(blk, aDiffic);
  }

  addBlock(blk, txsSizes, fee, blockSizes, alreadyGeneratedCoins);

  return true;
}

bool test_generator::constructBlockManuallyTx(cryptonote::Block& blk, const cryptonote::Block& prevBlock,
                                              const cryptonote::account_base& minerAcc,
                                              const std::vector<crypto::hash>& txHashes, size_t txsSize) {
  return constructBlockManually(blk, prevBlock, minerAcc, bf_tx_hashes, 0, 0, 0, crypto::hash(), 0, Transaction(),
    txHashes, txsSize);
}

bool test_generator::constructMaxSizeBlock(cryptonote::Block& blk, const cryptonote::Block& blkPrev,
                                           const cryptonote::account_base& minerAccount,
                                           size_t medianBlockCount/* = 0*/,
                                           const std::list<cryptonote::Transaction>& txList/* = std::list<cryptonote::Transaction>()*/) {
  std::vector<size_t> blockSizes;
  medianBlockCount = medianBlockCount == 0 ? m_currency.rewardBlocksWindow() : medianBlockCount;
  getLastNBlockSizes(blockSizes, get_block_hash(blkPrev), medianBlockCount);

  size_t median = misc_utils::median(blockSizes);
  size_t blockGrantedFullRewardZone = defaultMajorVersion <= BLOCK_MAJOR_VERSION_1 ?
    cryptonote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1 :
    m_currency.blockGrantedFullRewardZone();
  median = std::max(median, blockGrantedFullRewardZone);

  uint64_t totalFee = 0;
  size_t txsSize = 0;
  std::vector<crypto::hash> txHashes;
  for (auto& tx : txList) {
    uint64_t fee = 0;
    bool r = get_tx_fee(tx, fee);
    CHECK_AND_ASSERT_MES(r, false, "wrong transaction passed to construct_max_size_block");
    totalFee += fee;
    txsSize += get_object_blobsize(tx);
    txHashes.push_back(get_transaction_hash(tx));
  }

  Transaction minerTx;
  bool r = constructMinerTxBySize(m_currency, minerTx, get_block_height(blkPrev) + 1,
    getAlreadyGeneratedCoins(blkPrev), minerAccount.get_keys().m_account_address, blockSizes,
    2 * median - txsSize, 2 * median, totalFee, defaultMajorVersion > BLOCK_MAJOR_VERSION_1);
  if (!r) {
    return false;
  }

  return constructBlockManually(blk, blkPrev, minerAccount, test_generator::bf_miner_tx | test_generator::bf_tx_hashes,
    0, 0, 0, crypto::hash(), 0, minerTx, txHashes, txsSize, totalFee);
}

void fillNonce(cryptonote::Block& blk, const difficulty_type& diffic) {
  blk.nonce = 0;
  crypto::cn_context context;
  while (!miner::find_nonce_for_given_block(context, blk, diffic)) {
    blk.timestamp++;
  }
}

bool constructMinerTxManually(const cryptonote::Currency& currency, size_t height, uint64_t alreadyGeneratedCoins,
                              const AccountPublicAddress& minerAddress, Transaction& tx, uint64_t fee,
                              KeyPair* pTxKey/* = 0*/) {
  KeyPair txkey;
  txkey = KeyPair::generate();
  add_tx_pub_key_to_extra(tx, txkey.pub);

  if (0 != pTxKey) {
    *pTxKey = txkey;
  }

  TransactionInputGenerate in;
  in.height = height;
  tx.vin.push_back(in);

  // This will work, until size of constructed block is less then currency.blockGrantedFullRewardZone()
  int64_t emissionChange;
  uint64_t blockReward;
  if (!currency.getBlockReward(0, 0, alreadyGeneratedCoins, fee, false, blockReward, emissionChange)) {
    LOG_PRINT_L0("Block is too big");
    return false;
  }

  crypto::key_derivation derivation;
  crypto::public_key outEphPublicKey;
  crypto::generate_key_derivation(minerAddress.m_viewPublicKey, txkey.sec, derivation);
  crypto::derive_public_key(derivation, 0, minerAddress.m_spendPublicKey, outEphPublicKey);

  TransactionOutput out;
  out.amount = blockReward;
  out.target = TransactionOutputToKey(outEphPublicKey);
  tx.vout.push_back(out);

  tx.version = CURRENT_TRANSACTION_VERSION;
  tx.unlockTime = height + currency.minedMoneyUnlockWindow();

  return true;
}

bool constructMinerTxBySize(const cryptonote::Currency& currency, cryptonote::Transaction& minerTx, uint64_t height,
                            uint64_t alreadyGeneratedCoins, const cryptonote::AccountPublicAddress& minerAddress,
                            std::vector<size_t>& blockSizes, size_t targetTxSize, size_t targetBlockSize,
                            uint64_t fee/* = 0*/, bool penalizeFee/* = false*/) {
  if (!currency.constructMinerTx(height, misc_utils::median(blockSizes), alreadyGeneratedCoins, targetBlockSize,
      fee, minerAddress, minerTx, cryptonote::blobdata(), 1, penalizeFee)) {
    return false;
  }

  size_t currentSize = get_object_blobsize(minerTx);
  size_t tryCount = 0;
  while (targetTxSize != currentSize) {
    ++tryCount;
    if (10 < tryCount) {
      return false;
    }

    if (targetTxSize < currentSize) {
      size_t diff = currentSize - targetTxSize;
      if (diff <= minerTx.extra.size()) {
        minerTx.extra.resize(minerTx.extra.size() - diff);
      } else {
        return false;
      }
    } else {
      size_t diff = targetTxSize - currentSize;
      minerTx.extra.resize(minerTx.extra.size() + diff);
    }

    currentSize = get_object_blobsize(minerTx);
  }

  return true;
}
