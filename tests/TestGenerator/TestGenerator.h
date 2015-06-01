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

#pragma once

#include <cstdint>
#include <list>
#include <vector>
#include <unordered_map>

#include "crypto/hash.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/Currency.h"
#include "cryptonote_core/difficulty.h"


class test_generator
{
public:
  struct BlockInfo {
    BlockInfo()
      : prevId()
      , alreadyGeneratedCoins(0)
      , blockSize(0) {
    }

    BlockInfo(crypto::hash aPrevId, uint64_t anAlreadyGeneratedCoins, size_t aBlockSize)
      : prevId(aPrevId)
      , alreadyGeneratedCoins(anAlreadyGeneratedCoins)
      , blockSize(aBlockSize) {
    }

    crypto::hash prevId;
    uint64_t alreadyGeneratedCoins;
    size_t blockSize;
  };

  enum BlockFields {
    bf_none      = 0,
    bf_major_ver = 1 << 0,
    bf_minor_ver = 1 << 1,
    bf_timestamp = 1 << 2,
    bf_prev_id   = 1 << 3,
    bf_miner_tx  = 1 << 4,
    bf_tx_hashes = 1 << 5,
    bf_diffic    = 1 << 6
  };

  test_generator(const CryptoNote::Currency& currency, uint8_t majorVersion = CryptoNote::BLOCK_MAJOR_VERSION_1,
                 uint8_t minorVersion = CryptoNote::BLOCK_MINOR_VERSION_0)
    : m_currency(currency), defaultMajorVersion(majorVersion), defaultMinorVersion(minorVersion) {
  }


  uint8_t defaultMajorVersion;
  uint8_t defaultMinorVersion;

  const CryptoNote::Currency& currency() const { return m_currency; }

  void getBlockchain(std::vector<BlockInfo>& blockchain, const crypto::hash& head, size_t n) const;
  void getLastNBlockSizes(std::vector<size_t>& blockSizes, const crypto::hash& head, size_t n) const;
  uint64_t getAlreadyGeneratedCoins(const crypto::hash& blockId) const;
  uint64_t getAlreadyGeneratedCoins(const CryptoNote::Block& blk) const;

  void addBlock(const CryptoNote::Block& blk, size_t tsxSize, uint64_t fee, std::vector<size_t>& blockSizes,
    uint64_t alreadyGeneratedCoins);
  bool constructBlock(CryptoNote::Block& blk, uint32_t height, const crypto::hash& prevId,
    const CryptoNote::account_base& minerAcc, uint64_t timestamp, uint64_t alreadyGeneratedCoins,
    std::vector<size_t>& blockSizes, const std::list<CryptoNote::Transaction>& txList);
  bool constructBlock(CryptoNote::Block& blk, const CryptoNote::account_base& minerAcc, uint64_t timestamp);
  bool constructBlock(CryptoNote::Block& blk, const CryptoNote::Block& blkPrev, const CryptoNote::account_base& minerAcc,
    const std::list<CryptoNote::Transaction>& txList = std::list<CryptoNote::Transaction>());

  bool constructBlockManually(CryptoNote::Block& blk, const CryptoNote::Block& prevBlock,
    const CryptoNote::account_base& minerAcc, int actualParams = bf_none, uint8_t majorVer = 0,
    uint8_t minorVer = 0, uint64_t timestamp = 0, const crypto::hash& prevId = crypto::hash(),
    const CryptoNote::difficulty_type& diffic = 1, const CryptoNote::Transaction& minerTx = CryptoNote::Transaction(),
    const std::vector<crypto::hash>& txHashes = std::vector<crypto::hash>(), size_t txsSizes = 0, uint64_t fee = 0);
  bool constructBlockManuallyTx(CryptoNote::Block& blk, const CryptoNote::Block& prevBlock,
    const CryptoNote::account_base& minerAcc, const std::vector<crypto::hash>& txHashes, size_t txsSize);
  bool constructMaxSizeBlock(CryptoNote::Block& blk, const CryptoNote::Block& blkPrev,
    const CryptoNote::account_base& minerAccount, size_t medianBlockCount = 0,
    const std::list<CryptoNote::Transaction>& txList = std::list<CryptoNote::Transaction>());

private:
  const CryptoNote::Currency& m_currency;
  std::unordered_map<crypto::hash, BlockInfo> m_blocksInfo;
};

inline CryptoNote::difficulty_type getTestDifficulty() { return 1; }
void fillNonce(CryptoNote::Block& blk, const CryptoNote::difficulty_type& diffic);

bool constructMinerTxManually(const CryptoNote::Currency& currency, uint32_t height, uint64_t alreadyGeneratedCoins,
  const CryptoNote::AccountPublicAddress& minerAddress, CryptoNote::Transaction& tx, uint64_t fee,
  CryptoNote::KeyPair* pTxKey = 0);
bool constructMinerTxBySize(const CryptoNote::Currency& currency, CryptoNote::Transaction& minerTx, uint32_t height,
  uint64_t alreadyGeneratedCoins, const CryptoNote::AccountPublicAddress& minerAddress,
  std::vector<size_t>& blockSizes, size_t targetTxSize, size_t targetBlockSize, uint64_t fee = 0,
  bool penalizeFee = false);
