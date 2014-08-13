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

#include "crypto/crypto.h"
#include "cryptonote_core/cryptonote_basic.h"

#include "single_tx_test_base.h"

class test_generate_key_image : public single_tx_test_base
{
public:
  static const size_t loop_count = 1000;

  bool init()
  {
    using namespace cryptonote;

    if (!single_tx_test_base::init())
      return false;

    account_keys bob_keys = m_bob.get_keys();

    crypto::key_derivation recv_derivation;
    crypto::generate_key_derivation(m_tx_pub_key, bob_keys.m_view_secret_key, recv_derivation);

    crypto::derive_public_key(recv_derivation, 0, bob_keys.m_account_address.m_spendPublicKey, m_in_ephemeral.pub);
    crypto::derive_secret_key(recv_derivation, 0, bob_keys.m_spend_secret_key, m_in_ephemeral.sec);

    return true;
  }

  bool test()
  {
    crypto::key_image ki;
    crypto::generate_key_image(m_in_ephemeral.pub, m_in_ephemeral.sec, ki);
    return true;
  }

private:
  cryptonote::KeyPair m_in_ephemeral;
};
