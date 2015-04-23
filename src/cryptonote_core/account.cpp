// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "account.h"

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <fstream>

#include "include_base_utils.h"
#include "warnings.h"

DISABLE_VS_WARNINGS(4244 4345)

namespace cryptonote
{
  //-----------------------------------------------------------------
  account_base::account_base()
  {
    set_null();
  }
  //-----------------------------------------------------------------
  void account_base::set_null()
  {
    m_keys = account_keys();
  }
  //-----------------------------------------------------------------
  void account_base::generate()
  {
    crypto::generate_keys(m_keys.m_account_address.m_spendPublicKey, m_keys.m_spend_secret_key);
    crypto::generate_keys(m_keys.m_account_address.m_viewPublicKey, m_keys.m_view_secret_key);
    m_creation_timestamp = time(NULL);
  }
  //-----------------------------------------------------------------
  const account_keys& account_base::get_keys() const
  {
    return m_keys;
  }

  void account_base::set_keys(const account_keys& keys) {
    m_keys = keys;
  }
  //-----------------------------------------------------------------
}
