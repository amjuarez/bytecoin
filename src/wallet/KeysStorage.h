// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "crypto/crypto.h"

#include <stdint.h>

namespace cryptonote {

class ISerializer;

struct KeysStorage {
  uint64_t creationTimestamp;

  crypto::public_key spendPublicKey;
  crypto::secret_key spendSecretKey;

  crypto::public_key viewPublicKey;
  crypto::secret_key viewSecretKey;

  void serialize(ISerializer& serializer, const std::string& name);
};

} //namespace cryptonote
