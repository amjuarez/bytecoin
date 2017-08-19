// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#if defined(__cplusplus)
#include "crypto/crypto.h"

extern "C" {
#endif

void setup_random(void);

#if defined(__cplusplus)
}

bool check_scalar(const Crypto::EllipticCurveScalar &scalar);
void random_scalar(Crypto::EllipticCurveScalar &res);
void hash_to_scalar(const void *data, size_t length, Crypto::EllipticCurveScalar &res);
void hash_to_point(const Crypto::Hash &h, Crypto::EllipticCurvePoint &res);
void hash_to_ec(const Crypto::PublicKey &key, Crypto::EllipticCurvePoint &res);
#endif
