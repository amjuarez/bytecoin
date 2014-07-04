// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "checkpoints.h"
#include "misc_log_ex.h"

#define ADD_CHECKPOINT(h, hash)  CHECK_AND_ASSERT(checkpoints.add_checkpoint(h,  hash), false);

namespace cryptonote {
  inline bool create_checkpoints(cryptonote::checkpoints& checkpoints)
  {
    ADD_CHECKPOINT(1100,  "990a83b3e77ba5def86311da34793e09fa3b0a2875571bd59449173fddf4e129");
    ADD_CHECKPOINT(4200,  "76af92fc41eadf9c99df91efc08011d0fce6f3f55b131da2449c187f432f91f7");
    ADD_CHECKPOINT(11000, "970c15459e4d484166c36e303fcf35886e14633b40b1fe4e3f250eb03eaca1f8");
    
    return true;
  }
}
