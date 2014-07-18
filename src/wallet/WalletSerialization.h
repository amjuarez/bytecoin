// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <boost/serialization/split_free.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/array.hpp>

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
  epee::serialization::load_t_from_binary(account, data);
}

template<class Archive>
inline void save(Archive & ar, const cryptonote::account_base& account, const unsigned int version)
{
  std::string data;
  epee::serialization::store_t_to_binary(account, data);
  ar << data;
}

template<class Archive>
inline void serialize(Archive & ar, CryptoNote::Transaction& tx, const unsigned int version)
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
