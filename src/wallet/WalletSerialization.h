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

#include <boost/serialization/split_free.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/array.hpp>

#include "cryptonote_core/AccountKVSerialization.h"
#include "cryptonote_core/cryptonote_boost_serialization.h"
#include "common/unordered_containers_boost_serialization.h"
#include "storages/portable_storage_template_helper.h"

BOOST_SERIALIZATION_SPLIT_FREE(cryptonote::account_base);

namespace boost {
namespace serialization {

template<class Archive>
inline void load(Archive & ar, cryptonote::account_base& account, const unsigned int version)
{
  std::string data;
  ar >> data;
  cryptonote::AccountBaseSerializer<false> accountSerializer(account);
  epee::serialization::load_t_from_binary(accountSerializer, data);
}

template<class Archive>
inline void save(Archive & ar, const cryptonote::account_base& account, const unsigned int version)
{
  std::string data;
  cryptonote::AccountBaseSerializer<true> accountSerializer(account);
  epee::serialization::store_t_to_binary(accountSerializer, data);
  ar << data;
}

template<class Archive>
inline void serialize(Archive & ar, CryptoNote::TransactionInfo& tx, const unsigned int version)
{
  ar & tx.firstTransferId;
  ar & tx.transferCount;
  ar & tx.totalAmount;
  ar & tx.fee;
  ar & make_array(tx.hash.data(), tx.hash.size());
  ar & tx.isCoinbase;
  ar & tx.blockHeight;
  ar & tx.timestamp;
  ar & tx.extra;
}

template<class Archive>
inline void serialize(Archive & ar, CryptoNote::Transfer& tr, const unsigned int version)
{
  ar & tr.address;
  ar & tr.amount;
}

template<class Archive>
inline void serialize(Archive & ar, CryptoNote::TransferDetails& details, const unsigned int version)
{
  ar & details.blockHeight;
  ar & details.tx;
  ar & details.internalOutputIndex;
  ar & details.globalOutputIndex;
  ar & details.spent;
  ar & details.keyImage;
}

template<class Archive>
inline void serialize(Archive & ar, CryptoNote::UnconfirmedTransferDetails& details, const unsigned int version)
{
  ar & details.tx;
  ar & details.change;
  ar & details.sentTime;
  ar & details.transactionId;
}

} // namespace serialization
} // namespace boost
