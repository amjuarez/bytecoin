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
