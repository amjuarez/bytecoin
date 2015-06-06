// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <boost/utility.hpp>

#include "crypto/hash.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/difficulty.h"
#include "cryptonote_config.h"
#include "cryptonote_protocol/blobdatatype.h"

namespace cryptonote
{
  class Currency {
  public:
    uint64_t maxBlockHeight() const { return m_maxBlockHeight; }
    size_t maxBlockBlobSize() const { return m_maxBlockBlobSize; }
    size_t maxTxSize() const { return m_maxTxSize; }
    uint64_t publicAddressBase58Prefix() const { return m_publicAddressBase58Prefix; }
    size_t minedMoneyUnlockWindow() const { return m_minedMoneyUnlockWindow; }

    size_t timestampCheckWindow() const { return m_timestampCheckWindow; }
    uint64_t blockFutureTimeLimit() const { return m_blockFutureTimeLimit; }

    uint64_t moneySupply() const { return m_moneySupply; }

    size_t rewardBlocksWindow() const { return m_rewardBlocksWindow; }
    size_t blockGrantedFullRewardZone() const { return m_blockGrantedFullRewardZone; }
    size_t minerTxBlobReservedSize() const { return m_minerTxBlobReservedSize; }

    size_t numberOfDecimalPlaces() const { return m_numberOfDecimalPlaces; }
    uint64_t coin() const { return m_coin; }

    uint64_t minimumFee() const { return m_mininumFee; }
    uint64_t defaultDustThreshold() const { return m_defaultDustThreshold; }

    uint64_t difficultyTarget() const { return m_difficultyTarget; }
    size_t difficultyWindow() const { return m_difficultyWindow; }
    size_t difficultyLag() const { return m_difficultyLag; }
    size_t difficultyCut() const { return m_difficultyCut; }
    size_t difficultyBlocksCount() const { return m_difficultyWindow + m_difficultyLag; }

    uint64_t depositMinAmount() const { return m_depositMinAmount; }
    uint32_t depositMinTerm() const { return m_depositMinTerm; }
    uint32_t depositMaxTerm() const { return m_depositMaxTerm; }
    uint64_t depositMinTotalRateFactor() const { return m_depositMinTotalRateFactor; }
    uint64_t depositMaxTotalRate() const { return m_depositMaxTotalRate; }

    size_t maxBlockSizeInitial() const { return m_maxBlockSizeInitial; }
    uint64_t maxBlockSizeGrowthSpeedNumerator() const { return m_maxBlockSizeGrowthSpeedNumerator; }
    uint64_t maxBlockSizeGrowthSpeedDenominator() const { return m_maxBlockSizeGrowthSpeedDenominator; }

    uint64_t lockedTxAllowedDeltaSeconds() const { return m_lockedTxAllowedDeltaSeconds; }
    size_t lockedTxAllowedDeltaBlocks() const { return m_lockedTxAllowedDeltaBlocks; }

    uint64_t mempoolTxLiveTime() const { return m_mempoolTxLiveTime; }
    uint64_t mempoolTxFromAltBlockLiveTime() const { return m_mempoolTxFromAltBlockLiveTime; }

    uint64_t upgradeHeight() const { return m_upgradeHeight; }
    unsigned int upgradeVotingThreshold() const { return m_upgradeVotingThreshold; }
    size_t upgradeVotingWindow() const { return m_upgradeVotingWindow; }
    size_t upgradeWindow() const { return m_upgradeWindow; }
    size_t minNumberVotingBlocks() const { return (m_upgradeVotingWindow * m_upgradeVotingThreshold + 99) / 100; }
    uint64_t maxUpgradeDistance() const { return static_cast<uint64_t>(m_upgradeWindow); }
    uint64_t calculateUpgradeHeight(uint64_t voteCompleteHeight) const { return voteCompleteHeight + m_upgradeWindow; }

    const std::string& blocksFileName() const { return m_blocksFileName; }
    const std::string& blocksCacheFileName() const { return m_blocksCacheFileName; }
    const std::string& blockIndexesFileName() const { return m_blockIndexesFileName; }
    const std::string& txPoolFileName() const { return m_txPoolFileName; }

    bool isTestnet() const { return m_testnet; }

    const Block& genesisBlock() const { return m_genesisBlock; }
    const crypto::hash& genesisBlockHash() const { return m_genesisBlockHash; }

    bool getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee, uint64_t height,
                        uint64_t& reward, int64_t& emissionChange) const;
    uint64_t calculateInterest(uint64_t amount, uint32_t term) const;
    uint64_t calculateTotalTransactionInterest(const Transaction& tx) const;
    uint64_t getTransactionInputAmount(const TransactionInput& in) const;
    uint64_t getTransactionAllInputsAmount(const Transaction& tx) const;
    bool getTransactionFee(const Transaction& tx, uint64_t & fee) const;
    uint64_t getTransactionFee(const Transaction& tx) const;
    size_t maxBlockCumulativeSize(uint64_t height) const;

    bool constructMinerTx(size_t height, size_t medianSize, uint64_t alreadyGeneratedCoins, size_t currentBlockSize,
                          uint64_t fee, const AccountPublicAddress& minerAddress, Transaction& tx,
                          const blobdata& extraNonce = blobdata(), size_t maxOuts = 1) const;

    std::string accountAddressAsString(const account_base& account) const;
    bool parseAccountAddressString(const std::string& str, AccountPublicAddress& addr) const;

    std::string formatAmount(uint64_t amount) const;
    bool parseAmount(const std::string& str, uint64_t& amount) const;

    difficulty_type nextDifficulty(std::vector<uint64_t> timestamps, std::vector<difficulty_type> cumulativeDifficulties) const;
    bool checkProofOfWork(crypto::cn_context& context, const Block& block, difficulty_type currentDiffic, crypto::hash& proofOfWork) const;

  private:
    Currency() {
    }

    bool init();

    bool generateGenesisBlock();
    uint64_t baseRewardFunction(uint64_t alreadyGeneratedCoins, uint64_t height) const;

  private:
    uint64_t m_maxBlockHeight;
    size_t m_maxBlockBlobSize;
    size_t m_maxTxSize;
    uint64_t m_publicAddressBase58Prefix;
    size_t m_minedMoneyUnlockWindow;

    size_t m_timestampCheckWindow;
    uint64_t m_blockFutureTimeLimit;

    uint64_t m_moneySupply;

    size_t m_rewardBlocksWindow;
    size_t m_blockGrantedFullRewardZone;
    size_t m_minerTxBlobReservedSize;

    size_t m_numberOfDecimalPlaces;
    uint64_t m_coin;

    uint64_t m_mininumFee;
    uint64_t m_defaultDustThreshold;

    uint64_t m_difficultyTarget;
    size_t m_difficultyWindow;
    size_t m_difficultyLag;
    size_t m_difficultyCut;

    uint64_t m_depositMinAmount;
    uint32_t m_depositMinTerm;
    uint32_t m_depositMaxTerm;
    uint64_t m_depositMinTotalRateFactor;
    uint64_t m_depositMaxTotalRate;

    size_t m_maxBlockSizeInitial;
    uint64_t m_maxBlockSizeGrowthSpeedNumerator;
    uint64_t m_maxBlockSizeGrowthSpeedDenominator;

    uint64_t m_lockedTxAllowedDeltaSeconds;
    size_t m_lockedTxAllowedDeltaBlocks;

    uint64_t m_mempoolTxLiveTime;
    uint64_t m_mempoolTxFromAltBlockLiveTime;

    uint64_t m_upgradeHeight;
    unsigned int m_upgradeVotingThreshold;
    size_t m_upgradeVotingWindow;
    size_t m_upgradeWindow;

    std::string m_blocksFileName;
    std::string m_blocksCacheFileName;
    std::string m_blockIndexesFileName;
    std::string m_txPoolFileName;

    bool m_testnet;

    Block m_genesisBlock;
    crypto::hash m_genesisBlockHash;

    friend class CurrencyBuilder;
  };

  class CurrencyBuilder : boost::noncopyable {
  public:
    CurrencyBuilder();

    Currency currency() {
      if (!m_currency.init()) {
        throw std::runtime_error("Failed to initialize currency object");
      }
      return m_currency;
    }

    Transaction generateGenesisTransaction();

    CurrencyBuilder& maxBlockNumber(uint64_t val) { m_currency.m_maxBlockHeight = val; return *this; }
    CurrencyBuilder& maxBlockBlobSize(size_t val) { m_currency.m_maxBlockBlobSize = val; return *this; }
    CurrencyBuilder& maxTxSize(size_t val) { m_currency.m_maxTxSize = val; return *this; }
    CurrencyBuilder& publicAddressBase58Prefix(uint64_t val) { m_currency.m_publicAddressBase58Prefix = val; return *this; }
    CurrencyBuilder& minedMoneyUnlockWindow(size_t val) { m_currency.m_minedMoneyUnlockWindow = val; return *this; }

    CurrencyBuilder& timestampCheckWindow(size_t val) { m_currency.m_timestampCheckWindow = val; return *this; }
    CurrencyBuilder& blockFutureTimeLimit(uint64_t val) { m_currency.m_blockFutureTimeLimit = val; return *this; }

    CurrencyBuilder& moneySupply(uint64_t val) { m_currency.m_moneySupply = val; return *this; }

    CurrencyBuilder& rewardBlocksWindow(size_t val) { m_currency.m_rewardBlocksWindow = val; return *this; }
    CurrencyBuilder& blockGrantedFullRewardZone(size_t val) { m_currency.m_blockGrantedFullRewardZone = val; return *this; }
    CurrencyBuilder& minerTxBlobReservedSize(size_t val) { m_currency.m_minerTxBlobReservedSize = val; return *this; }

    CurrencyBuilder& numberOfDecimalPlaces(size_t val);

    CurrencyBuilder& mininumFee(uint64_t val) { m_currency.m_mininumFee = val; return *this; }
    CurrencyBuilder& defaultDustThreshold(uint64_t val) { m_currency.m_defaultDustThreshold = val; return *this; }

    CurrencyBuilder& difficultyTarget(uint64_t val) { m_currency.m_difficultyTarget = val; return *this; }
    CurrencyBuilder& difficultyWindow(size_t val);
    CurrencyBuilder& difficultyLag(size_t val) { m_currency.m_difficultyLag = val; return *this; }
    CurrencyBuilder& difficultyCut(size_t val) { m_currency.m_difficultyCut = val; return *this; }

    CurrencyBuilder& depositMinAmount(uint64_t val) { m_currency.m_depositMinAmount = val; return *this; }
    CurrencyBuilder& depositMinTerm(uint32_t val) { m_currency.m_depositMinTerm = val; return *this; }
    CurrencyBuilder& depositMaxTerm(uint32_t val) { m_currency.m_depositMaxTerm = val; return *this; }
    CurrencyBuilder& depositMinTotalRateFactor(uint64_t val) { m_currency.m_depositMinTotalRateFactor = val; return *this; }
    CurrencyBuilder& depositMaxTotalRate(uint64_t val) { m_currency.m_depositMaxTotalRate = val; return *this; }

    CurrencyBuilder& maxBlockSizeInitial(size_t val) { m_currency.m_maxBlockSizeInitial = val; return *this; }
    CurrencyBuilder& maxBlockSizeGrowthSpeedNumerator(uint64_t val) { m_currency.m_maxBlockSizeGrowthSpeedNumerator = val; return *this; }
    CurrencyBuilder& maxBlockSizeGrowthSpeedDenominator(uint64_t val) { m_currency.m_maxBlockSizeGrowthSpeedDenominator = val; return *this; }

    CurrencyBuilder& lockedTxAllowedDeltaSeconds(uint64_t val) { m_currency.m_lockedTxAllowedDeltaSeconds = val; return *this; }
    CurrencyBuilder& lockedTxAllowedDeltaBlocks(size_t val) { m_currency.m_lockedTxAllowedDeltaBlocks = val; return *this; }

    CurrencyBuilder& mempoolTxLiveTime(uint64_t val) { m_currency.m_mempoolTxLiveTime = val; return *this; }
    CurrencyBuilder& mempoolTxFromAltBlockLiveTime(uint64_t val) { m_currency.m_mempoolTxFromAltBlockLiveTime = val; return *this; }

    CurrencyBuilder& upgradeHeight(uint64_t val) { m_currency.m_upgradeHeight = val; return *this; }
    CurrencyBuilder& upgradeVotingThreshold(unsigned int val);
    CurrencyBuilder& upgradeVotingWindow(size_t val) { m_currency.m_upgradeVotingWindow = val; return *this; }
    CurrencyBuilder& upgradeWindow(size_t val);

    CurrencyBuilder& blocksFileName(const std::string& val) { m_currency.m_blocksFileName = val; return *this; }
    CurrencyBuilder& blocksCacheFileName(const std::string& val) { m_currency.m_blocksCacheFileName = val; return *this; }
    CurrencyBuilder& blockIndexesFileName(const std::string& val) { m_currency.m_blockIndexesFileName = val; return *this; }
    CurrencyBuilder& txPoolFileName(const std::string& val) { m_currency.m_txPoolFileName = val; return *this; }

    CurrencyBuilder& testnet(bool val) { m_currency.m_testnet = val; return *this; }

  private:
    Currency m_currency;
  };
}
