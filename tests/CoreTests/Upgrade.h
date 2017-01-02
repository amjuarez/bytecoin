// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 
#include "Chaingen.h"

struct gen_upgrade : public test_chain_unit_base
{
  gen_upgrade();

  bool generate(std::vector<test_event_entry>& events) const;

  bool check_block_verification_context(const CryptoNote::block_verification_context& bvc, size_t eventIdx, const CryptoNote::Block& blk);

  bool markInvalidBlock(CryptoNote::core& c, size_t evIndex, const std::vector<test_event_entry>& events);
  bool checkBlockTemplateVersionIsV1(CryptoNote::core& c, size_t evIndex, const std::vector<test_event_entry>& events);
  bool checkBlockTemplateVersionIsV2(CryptoNote::core& c, size_t evIndex, const std::vector<test_event_entry>& events);

private:
  bool checkBeforeUpgrade(std::vector<test_event_entry>& events, test_generator& generator,
                          const CryptoNote::Block& parentBlock, const CryptoNote::AccountBase& minerAcc, bool checkReward) const;
  bool checkAfterUpgrade(std::vector<test_event_entry>& events, test_generator& generator,
                         const CryptoNote::Block& parentBlock, const CryptoNote::AccountBase& minerAcc) const;
  bool checkBlockTemplateVersion(CryptoNote::core& c, uint8_t expectedMajorVersion, uint8_t expectedMinorVersion);
  bool makeBlockTxV1(std::vector<test_event_entry>& events, test_generator& generator, CryptoNote::Block& lastBlock,
                     const CryptoNote::Block& parentBlock, const CryptoNote::AccountBase& minerAcc, const CryptoNote::AccountBase& to, size_t count,
                     uint8_t majorVersion, uint8_t minorVersion) const;
  bool makeBlockTxV2(std::vector<test_event_entry>& events, test_generator& generator, CryptoNote::Block& lastBlock,
                     const CryptoNote::Block& parentBlock, const CryptoNote::AccountBase& minerAcc, const CryptoNote::AccountBase& to, size_t count,
                     uint8_t majorVersion, uint8_t minorVersion, bool before = true) const;

private:
  CryptoNote::AccountBase to;
  size_t m_invalidBlockIndex;
  size_t m_checkBlockTemplateVersionCallCounter;
};
