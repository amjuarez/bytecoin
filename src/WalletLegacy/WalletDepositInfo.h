// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "IWallet.h"

namespace CryptoNote {

struct DepositInfo {
  Deposit deposit;
  uint32_t outputInTransaction;
};

}
