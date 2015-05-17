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
