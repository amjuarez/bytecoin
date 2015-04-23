// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_core/cryptonote_basic.h"
#include "crypto/crypto.h"

namespace cryptonote {
  template<bool is_store> struct AccountBaseSerializer;

  struct account_keys {
    AccountPublicAddress m_account_address;
    crypto::secret_key   m_spend_secret_key;
    crypto::secret_key   m_view_secret_key;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class account_base {
  public:
    account_base();
    void generate();

    const account_keys& get_keys() const;
    void set_keys(const account_keys& keys);

    uint64_t get_createtime() const { return m_creation_timestamp; }
    void set_createtime(uint64_t val) { m_creation_timestamp = val; }

    bool load(const std::string& file_path);
    bool store(const std::string& file_path);

    template <class t_archive>
    inline void serialize(t_archive &a, const unsigned int /*ver*/) {
      a & m_keys;
      a & m_creation_timestamp;
    }

  private:
    void set_null();
    account_keys m_keys;
    uint64_t m_creation_timestamp;

    friend struct AccountBaseSerializer<true>;
    friend struct AccountBaseSerializer<false>;
  };
}
