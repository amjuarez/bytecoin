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

#pragma once

#include <vector>

#include "serialization.h"
#include "debug_archive.h"
#include "crypto/chacha8.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"

// read
template <template <bool> class Archive>
bool do_serialize(Archive<false> &ar, std::vector<crypto::signature> &v)
{
  size_t cnt = v.size();
  v.clear();

  // very basic sanity check
  if (ar.remaining_bytes() < cnt*sizeof(crypto::signature)) {
    ar.stream().setstate(std::ios::failbit);
    return false;
  }

  v.reserve(cnt);
  for (size_t i = 0; i < cnt; i++) {
    v.resize(i+1);
    ar.serialize_blob(&(v[i]), sizeof(crypto::signature), "");
    if (!ar.stream().good())
      return false;
  }
  return true;
}

// write
template <template <bool> class Archive>
bool do_serialize(Archive<true> &ar, std::vector<crypto::signature> &v)
{
  if (0 == v.size()) return true;
  ar.begin_string();
  size_t cnt = v.size();
  for (size_t i = 0; i < cnt; i++) {
    ar.serialize_blob(&(v[i]), sizeof(crypto::signature), "");
    if (!ar.stream().good())
      return false;
  }
  ar.end_string();
  return true;
}

BLOB_SERIALIZER(crypto::chacha8_iv);
BLOB_SERIALIZER(crypto::hash);
BLOB_SERIALIZER(crypto::public_key);
BLOB_SERIALIZER(crypto::secret_key);
BLOB_SERIALIZER(crypto::key_derivation);
BLOB_SERIALIZER(crypto::key_image);
BLOB_SERIALIZER(crypto::signature);
VARIANT_TAG(debug_archive, crypto::hash, "hash");
VARIANT_TAG(debug_archive, crypto::public_key, "public_key");
VARIANT_TAG(debug_archive, crypto::secret_key, "secret_key");
VARIANT_TAG(debug_archive, crypto::key_derivation, "key_derivation");
VARIANT_TAG(debug_archive, crypto::key_image, "key_image");
VARIANT_TAG(debug_archive, crypto::signature, "signature");
