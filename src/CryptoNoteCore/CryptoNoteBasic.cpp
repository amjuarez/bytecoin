// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CryptoNoteBasic.h"
#include "crypto/crypto.h"

namespace CryptoNote {

KeyPair generateKeyPair() {
  KeyPair k;
  Crypto::generate_keys(k.publicKey, k.secretKey);
  return k;
}

}
