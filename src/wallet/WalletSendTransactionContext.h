// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <list>
#include <vector>

#include "cryptonote_core/cryptonote_basic.h"
#include "IWallet.h"

namespace CryptoNote {

struct TxDustPolicy
{
  uint64_t dustThreshold;
  bool addToFee;
  cryptonote::account_public_address addrForDust;

  TxDustPolicy(uint64_t a_dust_threshold = 0, bool an_add_to_fee = true, cryptonote::account_public_address an_addr_for_dust = cryptonote::account_public_address())
  : dustThreshold(a_dust_threshold)
  , addToFee(an_add_to_fee)
  , addrForDust(an_addr_for_dust)
  {
  }
};

struct SendTransactionContext
{
  TransactionId transactionId;
  std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> outs;
  uint64_t foundMoney;
  std::list<size_t> selectedTransfers;
  uint64_t unlockTimestamp;
  TxDustPolicy dustPolicy;
  uint64_t mixIn;
};

} //namespace CryptoNote
