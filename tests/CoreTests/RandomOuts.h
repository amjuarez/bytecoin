// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

#include "Chaingen.h"

struct GetRandomOutputs : public test_chain_unit_base
{
  GetRandomOutputs();

  // bool check_tx_verification_context(const CryptoNote::tx_verification_context& tvc, bool tx_added, size_t event_idx, const CryptoNote::Transaction& tx);
  // bool check_block_verification_context(const CryptoNote::block_verification_context& bvc, size_t event_idx, const CryptoNote::Block& block);
  // bool mark_last_valid_block(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

  bool generate(std::vector<test_event_entry>& events) const;


private:

  bool checkHalfUnlocked(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool checkFullyUnlocked(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

  bool request(CryptoNote::core& c, uint64_t amount, size_t mixin, CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp);

};
