// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/map.hpp>
#include <boost/foreach.hpp>
#include <boost/serialization/is_bitwise_serializable.hpp>
#include "cryptonote_basic.h"
#include "Common/unordered_containers_boost_serialization.h"
#include "crypto/crypto.h"

//namespace CryptoNote {
namespace boost
{
  namespace serialization
  {

  //---------------------------------------------------
  template <class Archive>
  inline void serialize(Archive &a, crypto::public_key &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::public_key)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::secret_key &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::secret_key)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::key_derivation &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::key_derivation)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::key_image &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::key_image)]>(x);
  }

  template <class Archive>
  inline void serialize(Archive &a, crypto::signature &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::signature)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::hash &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::hash)]>(x);
  }

  template <class Archive> void serialize(Archive& archive, CryptoNote::TransactionInputToScript&, unsigned int version) {
    assert(false);
  }

  template <class Archive> void serialize(Archive& archive, CryptoNote::TransactionInputToScriptHash&, unsigned int version) {
    assert(false);
  }

  template <class Archive> void serialize(Archive& archive, CryptoNote::TransactionOutputToScript&, unsigned int version) {
    assert(false);
  }

  template <class Archive> void serialize(Archive& archive, CryptoNote::TransactionOutputToScriptHash&, unsigned int version) {
    assert(false);
  }

  template <class Archive> void serialize(Archive& archive, CryptoNote::TransactionInputMultisignature &output, unsigned int version) {
    archive & output.amount;
    archive & output.signatures;
    archive & output.outputIndex;
  }

  template <class Archive> void serialize(Archive& archive, CryptoNote::TransactionOutputMultisignature &output, unsigned int version) {
    archive & output.keys;
    archive & output.requiredSignatures;
  }

  template <class Archive>
  inline void serialize(Archive &a, CryptoNote::TransactionOutputToKey &x, const boost::serialization::version_type ver)
  {
    a & x.key;
  }

  template <class Archive>
  inline void serialize(Archive &a, CryptoNote::TransactionInputGenerate &x, const boost::serialization::version_type ver)
  {
    a & x.height;
  }

  template <class Archive>
  inline void serialize(Archive &a, CryptoNote::TransactionInputToKey &x, const boost::serialization::version_type ver)
  {
    a & x.amount;
    a & x.keyOffsets;
    a & x.keyImage;
  }

  template <class Archive>
  inline void serialize(Archive &a, CryptoNote::TransactionOutput &x, const boost::serialization::version_type ver)
  {
    a & x.amount;
    a & x.target;
  }


  template <class Archive>
  inline void serialize(Archive &a, CryptoNote::Transaction &x, const boost::serialization::version_type ver)
  {
    a & x.version;
    a & x.unlockTime;
    a & x.vin;
    a & x.vout;
    a & x.extra;
    a & x.signatures;
  }


  template <class Archive>
  inline void serialize(Archive &a, CryptoNote::Block &b, const boost::serialization::version_type ver)
  {
    a & b.majorVersion;
    a & b.minorVersion;
    a & b.timestamp;
    a & b.prevId;
    a & b.nonce;
    //------------------
    a & b.minerTx;
    a & b.txHashes;
  }
}
}

//}
