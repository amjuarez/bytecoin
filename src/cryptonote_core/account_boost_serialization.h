// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "account.h"
#include "cryptonote_core/cryptonote_boost_serialization.h"

//namespace cryptonote {
namespace boost
{
  namespace serialization
  {
    template <class Archive>
    inline void serialize(Archive &a, cryptonote::account_keys &x, const boost::serialization::version_type ver)
    {
      a & x.m_account_address;
      a & x.m_spend_secret_key;
      a & x.m_view_secret_key;
    }

    template <class Archive>
    inline void serialize(Archive &a, cryptonote::AccountPublicAddress &x, const boost::serialization::version_type ver)
    {
      a & x.m_spendPublicKey;
      a & x.m_viewPublicKey;
    }

  }
}
