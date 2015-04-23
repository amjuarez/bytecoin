// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
