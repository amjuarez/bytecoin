// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#include "crypto/crypto.cpp"

#include "crypto-tests.h"

bool check_scalar(const crypto::ec_scalar &scalar) {
  return crypto::sc_check(crypto::operator &(scalar)) == 0;
}

void random_scalar(crypto::ec_scalar &res) {
  crypto::random_scalar(res);
}

void hash_to_scalar(const void *data, std::size_t length, crypto::ec_scalar &res) {
  crypto::hash_to_scalar(data, length, res);
}

void hash_to_point(const crypto::hash &h, crypto::ec_point &res) {
  crypto::ge_p2 point;
  crypto::ge_fromfe_frombytes_vartime(&point, reinterpret_cast<const unsigned char *>(&h));
  crypto::ge_tobytes(crypto::operator &(res), &point);
}

void hash_to_ec(const crypto::public_key &key, crypto::ec_point &res) {
  crypto::ge_p3 tmp;
  crypto::hash_to_ec(key, tmp);
  crypto::ge_p3_tobytes(crypto::operator &(res), &tmp);
}
