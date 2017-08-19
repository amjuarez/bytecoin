// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/crypto.cpp"

#include "crypto-tests.h"

bool check_scalar(const Crypto::EllipticCurveScalar &scalar) {
  return Crypto::sc_check(reinterpret_cast<const unsigned char*>(&scalar)) == 0;
}

void random_scalar(Crypto::EllipticCurveScalar &res) {
  Crypto::random_scalar(res);
}

void hash_to_scalar(const void *data, size_t length, Crypto::EllipticCurveScalar &res) {
  Crypto::hash_to_scalar(data, length, res);
}

void hash_to_point(const Crypto::Hash &h, Crypto::EllipticCurvePoint &res) {
  Crypto::ge_p2 point;
  Crypto::ge_fromfe_frombytes_vartime(&point, reinterpret_cast<const unsigned char *>(&h));
  Crypto::ge_tobytes(reinterpret_cast<unsigned char*>(&res), &point);
}

void hash_to_ec(const Crypto::PublicKey &key, Crypto::EllipticCurvePoint &res) {
  Crypto::ge_p3 tmp;
  Crypto::hash_to_ec(key, tmp);
  Crypto::ge_p3_tobytes(reinterpret_cast<unsigned char*>(&res), &tmp);
}
