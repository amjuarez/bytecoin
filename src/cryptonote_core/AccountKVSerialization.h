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

#include "cryptonote_core/account.h"

// epee
#include "serialization/keyvalue_serialization.h"

namespace cryptonote {
  template<bool is_store> struct AccountPublicAddressSerializer;
  template<bool is_store> struct AccountKeysSerializer;
  template<bool is_store> struct AccountBaseSerializer;

  template<>
  struct AccountPublicAddressSerializer<true> {
    const AccountPublicAddress& m_account_address;

    AccountPublicAddressSerializer(const AccountPublicAddress& account_address) : m_account_address(account_address) {
    }

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(m_account_address.m_spendPublicKey, "m_spend_public_key")
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(m_account_address.m_viewPublicKey, "m_view_public_key")
    END_KV_SERIALIZE_MAP()
  };

  template<>
  struct AccountPublicAddressSerializer<false> {
    AccountPublicAddress& m_account_address;

    AccountPublicAddressSerializer(AccountPublicAddress& account_address) : m_account_address(account_address) {
    }

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(m_account_address.m_spendPublicKey, "m_spend_public_key")
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(m_account_address.m_viewPublicKey, "m_view_public_key")
    END_KV_SERIALIZE_MAP()
  };

  template<>
  struct AccountKeysSerializer<true> {
    const account_keys& m_keys;

    AccountKeysSerializer(const account_keys& keys) : m_keys(keys) {
    }

    BEGIN_KV_SERIALIZE_MAP()
      AccountPublicAddressSerializer<is_store> addressSerializer(this_ref.m_keys.m_account_address);
      epee::serialization::selector<is_store>::serialize(addressSerializer, stg, hparent_section, "m_account_address");
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(m_keys.m_spend_secret_key, "m_spend_secret_key")
        KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(m_keys.m_view_secret_key, "m_view_secret_key")
    END_KV_SERIALIZE_MAP()
  };

  template<>
  struct AccountKeysSerializer<false> {
    account_keys& m_keys;

    AccountKeysSerializer(account_keys& keys) : m_keys(keys) {
    }

    BEGIN_KV_SERIALIZE_MAP()
      AccountPublicAddressSerializer<is_store> addressSerializer(this_ref.m_keys.m_account_address);
      epee::serialization::selector<is_store>::serialize(addressSerializer, stg, hparent_section, "m_account_address");
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(m_keys.m_spend_secret_key, "m_spend_secret_key")
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(m_keys.m_view_secret_key, "m_view_secret_key")
    END_KV_SERIALIZE_MAP()
  };

  template<>
  struct AccountBaseSerializer<true> {
    const account_base& m_account;

    AccountBaseSerializer(const account_base& account) : m_account(account) {
    }

    BEGIN_KV_SERIALIZE_MAP()
      AccountKeysSerializer<is_store> keysSerializer(this_ref.m_account.m_keys);
      epee::serialization::selector<is_store>::serialize(keysSerializer, stg, hparent_section, "m_keys");
      KV_SERIALIZE_N(m_account.m_creation_timestamp, "m_creation_timestamp")
    END_KV_SERIALIZE_MAP()
  };

  template<>
  struct AccountBaseSerializer<false> {
    account_base& m_account;

    AccountBaseSerializer(account_base& account) : m_account(account) {
    }

    BEGIN_KV_SERIALIZE_MAP()
      AccountKeysSerializer<is_store> keysSerializer(this_ref.m_account.m_keys);
      epee::serialization::selector<is_store>::serialize(keysSerializer, stg, hparent_section, "m_keys");
      KV_SERIALIZE_N(m_account.m_creation_timestamp, "m_creation_timestamp")
    END_KV_SERIALIZE_MAP()
  };
}
