// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
      m_upgradeHeight = 0;
      m_blocksFileName       = "testnet_" + m_blocksFileName;
      m_blocksCacheFileName  = "testnet_" + m_blocksCacheFileName;
      m_blockIndexesFileName = "testnet_" + m_blockIndexesFileName;
      m_txPoolFileName       = "testnet_" + m_txPoolFileName;
    }

    return true;
  }

  bool Currency::generateGenesisBlock() {
    m_genesisBlock = boost::value_initialized<Block>();

    // Hard code coinbase tx in genesis block, because "tru" generating tx use random, but genesis should be always the same
    std::string genesisCoinbaseTxHex = GENESIS_COINBASE_TX_HEX;

    blobdata minerTxBlob;
    epee::string_tools::parse_hexstr_to_binbuff(genesisCoinbaseTxHex, minerTxBlob);
    bool r = parse_and_validate_tx_from_blob(minerTxBlob, m_genesisBlock.minerTx);
    CHECK_AND_ASSERT_MES(r, false, "failed to parse coinbase tx from hard coded blob");
    m_genesisBlock.majorVersion = BLOCK_MAJOR_VERSION_1;
    m_genesisBlock.minorVersion = BLOCK_MINOR_VERSION_0;
    m_genesisBlock.timestamp = 0;
    m_genesisBlock.nonce = GENESIS_NONCE;
    if (m_testnet) {
      ++m_genesisBlock.nonce;
    }
    //miner::find_nonce_for_given_block(bl, 1, 0);

    return true;
  }

  uint64_t Currency::baseRewardFunction(uint64_t alreadyGeneratedCoins, uint64_t height) const {
    uint64_t base_reward = cryptonote::START_BLOCK_REWARD >> (height / cryptonote::REWARD_HALVING_INTERVAL);
    base_reward = (std::max)(base_reward, cryptonote::MIN_BLOCK_REWARD);
    base_reward = (std::min)(base_reward, m_moneySupply - alreadyGeneratedCoins);
    return base_reward;
  }

  bool Currency::getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins,
                                uint64_t fee, uint64_t height, uint64_t& reward, int64_t& emissionChange) const {
    assert(alreadyGeneratedCoins <= m_moneySupply);

    uint64_t baseReward = baseRewardFunction(alreadyGeneratedCoins, height);

    medianSize = std::max(medianSize, m_blockGrantedFullRewardZone);
    if (currentBlockSize > UINT64_C(2) * medianSize) {
      LOG_PRINT_L4("Block cumulative size is too big: " << currentBlockSize << ", expected less than " << 2 * medianSize);
      return false;
    }

    uint64_t penalizedBaseReward = getPenalizedAmount(baseReward, medianSize, currentBlockSize);

    emissionChange = penalizedBaseReward;
    reward = penalizedBaseReward + fee;

    return true;
  }

  uint64_t Currency::calculateInterest(uint64_t amount, uint32_t term) const {
    assert(m_depositMinTerm <= term && term <= m_depositMaxTerm);
    assert(static_cast<uint64_t>(term)* m_depositMaxTotalRate > m_depositMinTotalRateFactor);

    uint64_t a = static_cast<uint64_t>(term) * m_depositMaxTotalRate - m_depositMinTotalRateFactor;
    uint64_t bHi;
    uint64_t bLo = mul128(amount, a, &bHi);

    uint64_t interestHi;
    uint64_t interestLo;
    assert(std::numeric_limits<uint32_t>::max() / 100 > m_depositMaxTerm);
    div128_32(bHi, bLo, static_cast<uint32_t>(100 * m_depositMaxTerm), &interestHi, &interestLo);
    assert(interestHi == 0);

    return interestLo;
  }

  uint64_t Currency::calculateTotalTransactionInterest(const Transaction& tx) const {
    uint64_t interest = 0;
    for (const TransactionInput& input : tx.vin) {
      if (input.type() == typeid(TransactionInputMultisignature)) {
        const TransactionInputMultisignature& multisignatureInput = boost::get<TransactionInputMultisignature>(input);
        if (multisignatureInput.term != 0) {
          interest += calculateInterest(multisignatureInput.amount, multisignatureInput.term);
        }
      }
    }

    return interest;
  }

  uint64_t Currency::getTransactionInputAmount(const TransactionInput& in) const {
    if (in.type() == typeid(TransactionInputToKey)) {
      return boost::get<TransactionInputToKey>(in).amount;
    } else if (in.type() == typeid(TransactionInputMultisignature)) {
      const TransactionInputMultisignature& multisignatureInput = boost::get<TransactionInputMultisignature>(in);
      if (multisignatureInput.term == 0) {
        return multisignatureInput.amount;
      } else {
        return multisignatureInput.amount + calculateInterest(multisignatureInput.amount, multisignatureInput.term);
      }
    } else {
      assert(false);
      return 0;
    }
  }

  uint64_t Currency::getTransactionAllInputsAmount(const Transaction& tx) const {
    uint64_t amount = 0;
    for (const auto& in : tx.vin) {
      amount += getTransactionInputAmount(in);
    }
    return amount;
  }

  bool Currency::getTransactionFee(const Transaction& tx, uint64_t & fee) const {
    uint64_t amount_in = 0;
    uint64_t amount_out = 0;

    for (const auto& in : tx.vin) {
      amount_in += getTransactionInputAmount(in);
    }

    for (const auto& o : tx.vout) {
      amount_out += o.amount;
    }

    CHECK_AND_ASSERT_MES(amount_in >= amount_out, false, "transaction spend (" << amount_in << ") more than it has (" << amount_out << ")");
    fee = amount_in - amount_out;
    return true;
  }

  uint64_t Currency::getTransactionFee(const Transaction& tx) const {
    uint64_t r = 0;
    if (!getTransactionFee(tx, r)) {
      r = 0;
    }
    return r;
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
                                  const blobdata& extraNonce/* = blobdata()*/, size_t maxOuts/* = 1*/) const {
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
      if (!getBlockReward(medianSize, currentBlockSize, alreadyGeneratedCoins, fee, height, blockReward, emissionChange)) {
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

      tx.version = TRANSACTION_VERSION_1;
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

  bool Currency::checkProofOfWork(crypto::cn_context& context, const Block& block, difficulty_type currentDiffic, crypto::hash& proofOfWork) const {
    if (!get_block_longhash(context, block, proofOfWork)) {
      return false;
    }

    return check_hash(proofOfWork, currentDiffic);
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

    depositMinAmount(parameters::DEPOSIT_MIN_AMOUNT);
    depositMinTerm(parameters::DEPOSIT_MIN_TERM);
    depositMaxTerm(parameters::DEPOSIT_MAX_TERM);
    depositMinTotalRateFactor(parameters::DEPOSIT_MIN_TOTAL_RATE_FACTOR);
    depositMaxTotalRate(parameters::DEPOSIT_MAX_TOTAL_RATE);

    maxBlockSizeInitial(parameters::MAX_BLOCK_SIZE_INITIAL);
    maxBlockSizeGrowthSpeedNumerator(parameters::MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR);
    maxBlockSizeGrowthSpeedDenominator(parameters::MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR);

    lockedTxAllowedDeltaSeconds(parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS);
    lockedTxAllowedDeltaBlocks(parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);

    mempoolTxLiveTime(parameters::CRYPTONOTE_MEMPOOL_TX_LIVETIME);
    mempoolTxFromAltBlockLiveTime(parameters::CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME);

    upgradeHeight(parameters::UPGRADE_HEIGHT);
    upgradeVotingThreshold(parameters::UPGRADE_VOTING_THRESHOLD);
    upgradeVotingWindow(parameters::UPGRADE_VOTING_WINDOW);
    upgradeWindow(parameters::UPGRADE_WINDOW);

    blocksFileName(parameters::CRYPTONOTE_BLOCKS_FILENAME);
    blocksCacheFileName(parameters::CRYPTONOTE_BLOCKSCACHE_FILENAME);
    blockIndexesFileName(parameters::CRYPTONOTE_BLOCKINDEXES_FILENAME);
    txPoolFileName(parameters::CRYPTONOTE_POOLDATA_FILENAME);

    testnet(false);
  }

  Transaction CurrencyBuilder::generateGenesisTransaction() {
    cryptonote::Transaction tx;
    cryptonote::AccountPublicAddress ac = boost::value_initialized<cryptonote::AccountPublicAddress>();
    m_currency.constructMinerTx(0, 0, 0, 0, 0, ac, tx); // zero fee in genesis

    return tx;
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
