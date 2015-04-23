// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/difficulty.h"

namespace cryptonote {
  struct i_miner_handler {
    virtual bool handle_block_found(Block& b) = 0;
    virtual bool get_block_template(Block& b, const AccountPublicAddress& adr, difficulty_type& diffic, uint64_t& height, const blobdata& ex_nonce) = 0;

  protected:
    ~i_miner_handler(){};
  };
}
