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

#include "Currency.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

// epee
#include "include_base_utils.h"
#include "string_tools.h"

#include "common/base58.h"
#include "common/int-util.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/UpgradeDetector.h"

namespace cryptonote {
  bool Currency::init() {
    bool r;
    r = generateGenesisBlock();
    CHECK_AND_ASSERT_MES(r, false, "Failed to generate genesis block");

    r = get_block_hash(m_genesisBlock, m_genesisBlockHash);
    CHECK_AND_ASSERT_MES(r, false, "Failed to get genesis block hash");

    if (isTestnet()) {
      m_blocksFileName       = "testnet_" + m_blocksFileName;
      m_blocksCacheFileName  = "testnet_" + m_blocksCacheFileName;
      m_blockIndexesFileName = "testnet_" + m_blockIndexesFileName;
      m_txPoolFileName       = "testnet_" + m_txPoolFileName;
    }

    return true;
  }

  bool Currency::generateGenesisBlock() {
    m_genesisBlock = boost::value_initialized<Block>();

    //account_public_address ac = boost::value_initialized<AccountPublicAddress>();
    //std::vector<size_t> sz;
    //constructMinerTx(0, 0, 0, 0, 0, ac, m_genesisBlock.minerTx); // zero fee in genesis
    //blobdata txb = tx_to_blob(m_genesisBlock.minerTx);
    //std::string hex_tx_represent = string_tools::buff_to_hex_nodelimer(txb);

    // Hard code coinbase tx in genesis block, because through generating tx use random, but genesis should be always the same
    std::string genesisCoinbaseTxHex = "010a01ff0001ffffffffffff0f029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd08807121013c086a48c15fb637a96991bc6d53caf77068b5ba6eeb3c82357228c49790584a";

    blobdata minerTxBlob;
    epee::string_tools::parse_hexstr_to_binbuff(genesisCoinbaseTxHex, minerTxBlob);
    bool r = parse_and_validate_tx_from_blob(minerTxBlob, m_genesisBlock.minerTx);
    CHECK_AND_ASSERT_MES(r, false, "failed to parse coinbase tx from hard coded blob");
    m_genesisBlock.majorVersion = BLOCK_MAJOR_VERSION_1;
    m_genesisBlock.minorVersion = BLOCK_MINOR_VERSION_0;
    m_genesisBlock.timestamp = 0;
    m_genesisBlock.nonce = 70;
    if (m_testnet) {
      ++m_genesisBlock.nonce;
    }
    //miner::find_nonce_for_given_block(bl, 1, 0);

    return true;
  }

  bool Currency::getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins,
                                uint64_t fee, bool penalizeFee, uint64_t& reward, int64_t& emissionChange) const {
      assert(alreadyGeneratedCoins <= m_moneySupply);
      assert(m_emissionSpeedFactor > 0 && m_emissionSpeedFactor <= 8 * sizeof(uint64_t));

      uint64_t baseReward = (m_moneySupply - alreadyGeneratedCoins) >> m_emissionSpeedFactor;

      medianSize = std::max(medianSize, m_blockGrantedFullRewardZone);
      if (currentBlockSize > UINT64_C(2) * medianSize) {
        LOG_PRINT_L4("Block cumulative size is too big: " << currentBlockSize << ", expected less than " << 2 * medianSize);
        return false;
      }

      uint64_t penalizedBaseReward = getPenalizedAmount(baseReward, medianSize, currentBlockSize);
      uint64_t penalizedFee = penalizeFee ? getPenalizedAmount(fee, medianSize, currentBlockSize) : fee;

      emissionChange = penalizedBaseReward - (fee - penalizedFee);
      reward = penalizedBaseReward + penalizedFee;

      return true;
  }

  size_t Currency::maxBlockCumulativeSize(uint64_t height) const {
    assert(height <= std::numeric_limits<uint64_t>::max() / m_maxBlockSizeGrowthSpeedNumerator);
    size_t maxSize = static_cast<size_t>(m_maxBlockSizeInitial +
      (height * m_maxBlockSizeGrowthSpeedNumerator) / m_maxBlockSizeGrowthSpeedDenominator);
    assert(maxSize >= m_maxBlockSizeInitial);
    return maxSize;
  }

  bool Currency::constructMinerTx(size_t height, size_t medianSize, uint64_t alreadyGeneratedCoins, size_t currentBlockSize,
                                  uint64_t fee, const AccountPublicAddress& minerAddress, Transaction& tx,
                                  const blobdata& extraNonce/* = blobdata()*/, size_t maxOuts/* = 1*/,
                                  bool penalizeFee/* = false*/) const {
      tx.vin.clear();
      tx.vout.clear();
      tx.extra.clear();

      KeyPair txkey = KeyPair::generate();
      add_tx_pub_key_to_extra(tx, txkey.pub);
      if (!extraNonce.empty()) {
        if (!add_extra_nonce_to_tx_extra(tx.extra, extraNonce)) {
          return false;
        }
      }

      TransactionInputGenerate in;
      in.height = height;

      uint64_t blockReward;
      int64_t emissionChange;
      if (!getBlockReward(medianSize, currentBlockSize, alreadyGeneratedCoins, fee, penalizeFee, blockReward, emissionChange)) {
        LOG_PRINT_L0("Block is too big");
        return false;
      }
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
      LOG_PRINT_L1("Creating block template: reward " << blockReward << ", fee " << fee);
#endif

      std::vector<uint64_t> outAmounts;
      decompose_amount_into_digits(blockReward, m_defaultDustThreshold,
        [&outAmounts](uint64_t a_chunk) { outAmounts.push_back(a_chunk); },
        [&outAmounts](uint64_t a_dust) { outAmounts.push_back(a_dust); });

      CHECK_AND_ASSERT_MES(1 <= maxOuts, false, "max_out must be non-zero");
      while (maxOuts < outAmounts.size()) {
        outAmounts[outAmounts.size() - 2] += outAmounts.back();
        outAmounts.resize(outAmounts.size() - 1);
      }

      uint64_t summaryAmounts = 0;
      for (size_t no = 0; no < outAmounts.size(); no++) {
        crypto::key_derivation derivation = boost::value_initialized<crypto::key_derivation>();
        crypto::public_key outEphemeralPubKey = boost::value_initialized<crypto::public_key>();
        bool r = crypto::generate_key_derivation(minerAddress.m_viewPublicKey, txkey.sec, derivation);
        CHECK_AND_ASSERT_MES(r, false, "while creating outs: failed to generate_key_derivation(" <<
          minerAddress.m_viewPublicKey << ", " << txkey.sec << ")");

        r = crypto::derive_public_key(derivation, no, minerAddress.m_spendPublicKey, outEphemeralPubKey);
        CHECK_AND_ASSERT_MES(r, false, "while creating outs: failed to derive_public_key(" << derivation << ", " <<
          no << ", "<< minerAddress.m_spendPublicKey << ")");

        TransactionOutputToKey tk;
        tk.key = outEphemeralPubKey;

        TransactionOutput out;
        summaryAmounts += out.amount = outAmounts[no];
        out.target = tk;
        tx.vout.push_back(out);
      }

      CHECK_AND_ASSERT_MES(summaryAmounts == blockReward, false,
        "Failed to construct miner tx, summaryAmounts = " << summaryAmounts << " not equal blockReward = " << blockReward);

      tx.version = CURRENT_TRANSACTION_VERSION;
      //lock
      tx.unlockTime = height + m_minedMoneyUnlockWindow;
      tx.vin.push_back(in);
      return true;
  }

  std::string Currency::accountAddressAsString(const account_base& account) const {
    return getAccountAddressAsStr(m_publicAddressBase58Prefix, account.get_keys().m_account_address);
  }

  bool Currency::parseAccountAddressString(const std::string& str, AccountPublicAddress& addr) const {
    uint64_t prefix;
    if (!cryptonote::parseAccountAddressString(prefix, addr, str)) {
      return false;
    }

    if (prefix != m_publicAddressBase58Prefix) {
      LOG_PRINT_L1("Wrong address prefix: " << prefix << ", expected " << m_publicAddressBase58Prefix);
      return false;
    }

    return true;
  }

  std::string Currency::formatAmount(uint64_t amount) const {
    std::string s = std::to_string(amount);
    if (s.size() < m_numberOfDecimalPlaces + 1) {
      s.insert(0, m_numberOfDecimalPlaces + 1 - s.size(), '0');
    }
    s.insert(s.size() - m_numberOfDecimalPlaces, ".");
    return s;
  }

  bool Currency::parseAmount(const std::string& str, uint64_t& amount) const {
    std::string strAmount = str;
    boost::algorithm::trim(strAmount);

    size_t pointIndex = strAmount.find_first_of('.');
    size_t fractionSize;
    if (std::string::npos != pointIndex) {
      fractionSize = strAmount.size() - pointIndex - 1;
      while (m_numberOfDecimalPlaces < fractionSize && '0' == strAmount.back()) {
        strAmount.erase(strAmount.size() - 1, 1);
        --fractionSize;
      }
      if (m_numberOfDecimalPlaces < fractionSize) {
        return false;
      }
      strAmount.erase(pointIndex, 1);
    } else {
      fractionSize = 0;
    }

    if (strAmount.empty()) {
      return false;
    }

    if (fractionSize < m_numberOfDecimalPlaces) {
      strAmount.append(m_numberOfDecimalPlaces - fractionSize, '0');
    }

    return epee::string_tools::get_xtype_from_string(amount, strAmount);
  }

  difficulty_type Currency::nextDifficulty(std::vector<uint64_t> timestamps,
                                           std::vector<difficulty_type> cumulativeDifficulties) const {
    assert(m_difficultyWindow >= 2);

    if (timestamps.size() > m_difficultyWindow) {
      timestamps.resize(m_difficultyWindow);
      cumulativeDifficulties.resize(m_difficultyWindow);
    }

    size_t length = timestamps.size();
    assert(length == cumulativeDifficulties.size());
    assert(length <= m_difficultyWindow);
    if (length <= 1) {
      return 1;
    }

    sort(timestamps.begin(), timestamps.end());

    size_t cutBegin, cutEnd;
    assert(2 * m_difficultyCut <= m_difficultyWindow - 2);
    if (length <= m_difficultyWindow - 2 * m_difficultyCut) {
      cutBegin = 0;
      cutEnd = length;
    } else {
      cutBegin = (length - (m_difficultyWindow - 2 * m_difficultyCut) + 1) / 2;
      cutEnd = cutBegin + (m_difficultyWindow - 2 * m_difficultyCut);
    }
    assert(/*cut_begin >= 0 &&*/ cutBegin + 2 <= cutEnd && cutEnd <= length);
    uint64_t timeSpan = timestamps[cutEnd - 1] - timestamps[cutBegin];
    if (timeSpan == 0) {
      timeSpan = 1;
    }

    difficulty_type totalWork = cumulativeDifficulties[cutEnd - 1] - cumulativeDifficulties[cutBegin];
    assert(totalWork > 0);

    uint64_t low, high;
    low = mul128(totalWork, m_difficultyTarget, &high);
    if (high != 0 || low + timeSpan - 1 < low) {
      return 0;
    }

    return (low + timeSpan - 1) / timeSpan;
  }

  bool Currency::checkProofOfWorkV1(crypto::cn_context& context, const Block& block, difficulty_type currentDiffic,
                                    crypto::hash& proofOfWork) const {
    if (BLOCK_MAJOR_VERSION_1 != block.majorVersion) {
      return false;
    }

    if (!get_block_longhash(context, block, proofOfWork)) {
      return false;
    }

    return check_hash(proofOfWork, currentDiffic);
  }

  bool Currency::checkProofOfWorkV2(crypto::cn_context& context, const Block& block, difficulty_type currentDiffic,
                                    crypto::hash& proofOfWork) const {
    if (BLOCK_MAJOR_VERSION_2 != block.majorVersion) {
      return false;
    }

    if (!get_block_longhash(context, block, proofOfWork)) {
      return false;
    }

    if (!check_hash(proofOfWork, currentDiffic)) {
      return false;
    }

    tx_extra_merge_mining_tag mmTag;
    if (!get_mm_tag_from_extra(block.parentBlock.minerTx.extra, mmTag)) {
      LOG_ERROR("merge mining tag wasn't found in extra of the parent block miner transaction");
      return false;
    }

    if (8 * sizeof(m_genesisBlockHash) < block.parentBlock.blockchainBranch.size()) {
      return false;
    }

    crypto::hash auxBlockHeaderHash;
    if (!get_aux_block_header_hash(block, auxBlockHeaderHash)) {
      return false;
    }

    crypto::hash auxBlocksMerkleRoot;
    crypto::tree_hash_from_branch(block.parentBlock.blockchainBranch.data(), block.parentBlock.blockchainBranch.size(),
      auxBlockHeaderHash, &m_genesisBlockHash, auxBlocksMerkleRoot);
    CHECK_AND_NO_ASSERT_MES(auxBlocksMerkleRoot == mmTag.merkle_root, false, "Aux block hash wasn't found in merkle tree");

    return true;
  }

  bool Currency::checkProofOfWork(crypto::cn_context& context, const Block& block, difficulty_type currentDiffic, crypto::hash& proofOfWork) const {
    switch (block.majorVersion) {
    case BLOCK_MAJOR_VERSION_1: return checkProofOfWorkV1(context, block, currentDiffic, proofOfWork);
    case BLOCK_MAJOR_VERSION_2: return checkProofOfWorkV2(context, block, currentDiffic, proofOfWork);
    }

    CHECK_AND_ASSERT_MES(false, false, "Unknown block major version: " << block.majorVersion << "." << block.minorVersion);
  }

  CurrencyBuilder::CurrencyBuilder() {
    maxBlockNumber(parameters::CRYPTONOTE_MAX_BLOCK_NUMBER);
    maxBlockBlobSize(parameters::CRYPTONOTE_MAX_BLOCK_BLOB_SIZE);
    maxTxSize(parameters::CRYPTONOTE_MAX_TX_SIZE);
    publicAddressBase58Prefix(parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);
    minedMoneyUnlockWindow(parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);

    timestampCheckWindow(parameters::BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW);
    blockFutureTimeLimit(parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT);

    moneySupply(parameters::MONEY_SUPPLY);
    emissionSpeedFactor(parameters::EMISSION_SPEED_FACTOR);

    rewardBlocksWindow(parameters::CRYPTONOTE_REWARD_BLOCKS_WINDOW);
    blockGrantedFullRewardZone(parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
    minerTxBlobReservedSize(parameters::CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE);

    numberOfDecimalPlaces(parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT);

    mininumFee(parameters::MINIMUM_FEE);
    defaultDustThreshold(parameters::DEFAULT_DUST_THRESHOLD);

    difficultyTarget(parameters::DIFFICULTY_TARGET);
    difficultyWindow(parameters::DIFFICULTY_WINDOW);
    difficultyLag(parameters::DIFFICULTY_LAG);
    difficultyCut(parameters::DIFFICULTY_CUT);

    maxBlockSizeInitial(parameters::MAX_BLOCK_SIZE_INITIAL);
    maxBlockSizeGrowthSpeedNumerator(parameters::MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR);
    maxBlockSizeGrowthSpeedDenominator(parameters::MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR);

    lockedTxAllowedDeltaSeconds(parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS);
    lockedTxAllowedDeltaBlocks(parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);

    mempoolTxLiveTime(parameters::CRYPTONOTE_MEMPOOL_TX_LIVETIME);
    mempoolTxFromAltBlockLiveTime(parameters::CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME);

    upgradeHeight(UpgradeDetectorBase::UNDEF_HEIGHT);
    upgradeVotingThreshold(parameters::UPGRADE_VOTING_THRESHOLD);
    upgradeVotingWindow(parameters::UPGRADE_VOTING_WINDOW);
    upgradeWindow(parameters::UPGRADE_WINDOW);

    blocksFileName(parameters::CRYPTONOTE_BLOCKS_FILENAME);
    blocksCacheFileName(parameters::CRYPTONOTE_BLOCKSCACHE_FILENAME);
    blockIndexesFileName(parameters::CRYPTONOTE_BLOCKINDEXES_FILENAME);
    txPoolFileName(parameters::CRYPTONOTE_POOLDATA_FILENAME);

    testnet(false);
  }

  CurrencyBuilder& CurrencyBuilder::emissionSpeedFactor(unsigned int val) {
    if (val <= 0 || val > 8 * sizeof(uint64_t)) {
      throw std::invalid_argument("val at emissionSpeedFactor()");
    }

    m_currency.m_emissionSpeedFactor = val;
    return *this;
  }

  CurrencyBuilder& CurrencyBuilder::numberOfDecimalPlaces(size_t val) {
    m_currency.m_numberOfDecimalPlaces = val;
    m_currency.m_coin = 1;
    for (size_t i = 0; i < m_currency.m_numberOfDecimalPlaces; ++i) {
      m_currency.m_coin *= 10;
    }

    return *this;
  }

  CurrencyBuilder& CurrencyBuilder::difficultyWindow(size_t val) {
    if (val < 2) {
      throw std::invalid_argument("val at difficultyWindow()");
    }
    m_currency.m_difficultyWindow = val;
    return *this;
  }

  CurrencyBuilder& CurrencyBuilder::upgradeVotingThreshold(unsigned int val) {
    if (val <= 0 || val > 100) {
      throw std::invalid_argument("val at upgradeVotingThreshold()");
    }
    m_currency.m_upgradeVotingThreshold = val;
    return *this;
  }

  CurrencyBuilder& CurrencyBuilder::upgradeWindow(size_t val) {
    if (val <= 0) {
      throw std::invalid_argument("val at upgradeWindow()");
    }
    m_currency.m_upgradeWindow = val;
    return *this;
  }
}
