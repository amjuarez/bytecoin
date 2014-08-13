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

#include <algorithm>
#include <cstdint>
#include <ctime>

// epee
#include "include_base_utils.h"

#include "cryptonote_core/Currency.h"
#include "cryptonote_config.h"

namespace cryptonote {
  class UpgradeDetectorBase {
  public:
    enum : uint64_t {
      UNDEF_HEIGHT = static_cast<uint64_t>(-1),
    };
  };

  static_assert(cryptonote::UpgradeDetectorBase::UNDEF_HEIGHT == UINT64_C(0xFFFFFFFFFFFFFFFF), "UpgradeDetectorBase::UNDEF_HEIGHT has invalid value");

  template <typename BC>
  class BasicUpgradeDetector : public UpgradeDetectorBase {
  public:
    BasicUpgradeDetector(const Currency& currency, BC& blockchain, uint8_t targetVersion) :
      m_currency(currency),
      m_blockchain(blockchain),
      m_targetVersion(targetVersion),
      m_votingCompleteHeight(UNDEF_HEIGHT) {
    }

    bool init() {
      if (m_currency.upgradeHeight() == UNDEF_HEIGHT) {
        if (m_blockchain.empty()) {
          m_votingCompleteHeight = UNDEF_HEIGHT;

        } else if (m_targetVersion - 1 == m_blockchain.back().bl.majorVersion) {
          m_votingCompleteHeight = findVotingCompleteHeight(m_blockchain.size() - 1);

        } else if (m_targetVersion <= m_blockchain.back().bl.majorVersion) {
          auto it = std::lower_bound(m_blockchain.begin(), m_blockchain.end(), m_targetVersion,
            [](const typename BC::value_type& b, uint8_t v) { return b.bl.majorVersion < v; });
          CHECK_AND_ASSERT_MES(it != m_blockchain.end() && it->bl.majorVersion == m_targetVersion, false,
            "Internal error: upgrade height isn't found");
          uint64_t upgradeHeight = it - m_blockchain.begin();
          m_votingCompleteHeight = findVotingCompleteHeight(upgradeHeight);
          CHECK_AND_ASSERT_MES(m_votingCompleteHeight != UNDEF_HEIGHT, false,
            "Internal error: voting complete height isn't found, upgrade height = " << upgradeHeight);

        } else {
          m_votingCompleteHeight = UNDEF_HEIGHT;
        }
      } else if (!m_blockchain.empty()) {
        if (m_blockchain.size() <= m_currency.upgradeHeight() + 1) {
          CHECK_AND_ASSERT_MES(m_blockchain.back().bl.majorVersion == m_targetVersion - 1, false,
            "Internal error: block at height " << (m_blockchain.size() - 1) << " has invalid version " <<
            static_cast<int>(m_blockchain.back().bl.majorVersion) << ", expected " << static_cast<int>(m_targetVersion));
        } else {
          int blockVersionAtUpgradeHeight = m_blockchain[m_currency.upgradeHeight()].bl.majorVersion;
          CHECK_AND_ASSERT_MES(blockVersionAtUpgradeHeight == m_targetVersion - 1, false,
            "Internal error: block at height " << m_currency.upgradeHeight() << " has invalid version " <<
            blockVersionAtUpgradeHeight << ", expected " << static_cast<int>(m_targetVersion - 1));

          int blockVersionAfterUpgradeHeight = m_blockchain[m_currency.upgradeHeight() + 1].bl.majorVersion;
          CHECK_AND_ASSERT_MES(blockVersionAfterUpgradeHeight == m_targetVersion, false,
            "Internal error: block at height " << (m_currency.upgradeHeight() + 1) << " has invalid version " <<
            blockVersionAfterUpgradeHeight << ", expected " << static_cast<int>(m_targetVersion));
        }
      }

      return true;
    }

    uint8_t targetVersion() const { return m_targetVersion; }
    uint64_t votingCompleteHeight() const { return m_votingCompleteHeight; }

    uint64_t upgradeHeight() const {
      if (m_currency.upgradeHeight() == UNDEF_HEIGHT) {
        return m_votingCompleteHeight == UNDEF_HEIGHT ? UNDEF_HEIGHT : m_currency.calculateUpgradeHeight(m_votingCompleteHeight);
      } else {
        return m_currency.upgradeHeight();
      }
    }

    void blockPushed() {
      assert(!m_blockchain.empty());

      if (m_currency.upgradeHeight() != UNDEF_HEIGHT) {
        if (m_blockchain.size() <= m_currency.upgradeHeight() + 1) {
          assert(m_blockchain.back().bl.majorVersion == m_targetVersion - 1);
        } else {
          assert(m_blockchain.back().bl.majorVersion == m_targetVersion);
        }

      } else if (m_votingCompleteHeight != UNDEF_HEIGHT) {
        assert(m_blockchain.size() > m_votingCompleteHeight);

        if (m_blockchain.size() <= upgradeHeight()) {
          assert(m_blockchain.back().bl.majorVersion == m_targetVersion - 1);

          if (m_blockchain.size() % (60 * 60 / m_currency.difficultyTarget()) == 0) {
            LOG_PRINT_GREEN("###### UPGRADE is going to happen after height " << upgradeHeight() << "!", LOG_LEVEL_2);
          }
        } else if (m_blockchain.size() == upgradeHeight() + 1) {
          assert(m_blockchain.back().bl.majorVersion == m_targetVersion - 1);

          LOG_PRINT_GREEN("###### UPGRADE has happened! Starting from height " << (upgradeHeight() + 1) <<
            " blocks with major version below " << static_cast<int>(m_targetVersion) << " will be rejected!", LOG_LEVEL_2);
        } else {
          assert(m_blockchain.back().bl.majorVersion == m_targetVersion);
        }

      } else {
        uint64_t lastBlockHeight = m_blockchain.size() - 1;
        if (isVotingComplete(lastBlockHeight)) {
          m_votingCompleteHeight = lastBlockHeight;
          LOG_PRINT_GREEN("###### UPGRADE voting complete at height " << m_votingCompleteHeight <<
            "! UPGRADE is going to happen after height " << upgradeHeight() << "!", LOG_LEVEL_2);
        }
      }
    }

    void blockPopped() {
      if (m_votingCompleteHeight != UNDEF_HEIGHT) {
        assert(m_currency.upgradeHeight() == UNDEF_HEIGHT);

        if (m_blockchain.size() == m_votingCompleteHeight) {
          LOG_PRINT_YELLOW("###### UPGRADE after height " << upgradeHeight() << " has been cancelled!", LOG_LEVEL_2);
          m_votingCompleteHeight = UNDEF_HEIGHT;
        } else {
          assert(m_blockchain.size() > m_votingCompleteHeight);
        }
      }
    }

  private:
    uint64_t findVotingCompleteHeight(uint64_t probableUpgradeHeight) {
      assert(m_currency.upgradeHeight() == UNDEF_HEIGHT);

      uint64_t probableVotingCompleteHeight = probableUpgradeHeight > m_currency.maxUpgradeDistance() ?
        probableUpgradeHeight - m_currency.maxUpgradeDistance() : 0;
      for (size_t i = probableVotingCompleteHeight; i <= probableUpgradeHeight; ++i) {
        if (isVotingComplete(i)) {
          return i;
        }
      }

      return UNDEF_HEIGHT;
    }

    bool isVotingComplete(uint64_t height) {
      assert(m_currency.upgradeHeight() == UNDEF_HEIGHT);
      assert(m_currency.upgradeVotingWindow() > 1);
      assert(m_currency.upgradeVotingThreshold() > 0 && m_currency.upgradeVotingThreshold() <= 100);

      if (height < static_cast<uint64_t>(m_currency.upgradeVotingWindow()) - 1) {
        return false;
      }

      unsigned int voteCounter = 0;
      for (size_t i = height + 1 - m_currency.upgradeVotingWindow(); i <= height; ++i) {
        const auto& b = m_blockchain[i].bl;
        voteCounter += (b.majorVersion == m_targetVersion - 1) && (b.minorVersion == BLOCK_MINOR_VERSION_1) ? 1 : 0;
      }

      return m_currency.upgradeVotingThreshold() * m_currency.upgradeVotingWindow() <= 100 * voteCounter;
    }

  private:
    const Currency& m_currency;
    BC& m_blockchain;
    uint8_t m_targetVersion;
    uint64_t m_votingCompleteHeight;
  };
}
