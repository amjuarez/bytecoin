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

#include "cryptonote_basic.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"


namespace cryptonote {
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  template<class t_array>
  struct array_hasher: std::unary_function<t_array&, std::size_t>
  {
    std::size_t operator()(const t_array& val) const
    {
      return boost::hash_range(&val.data[0], &val.data[sizeof(val.data)]);
    }
  };


#pragma pack(push, 1)
  struct public_address_outer_blob
  {
    uint8_t m_ver;
    account_public_address m_address;
    uint8_t check_sum;
  };
#pragma pack (pop)


  /************************************************************************/
  /* Cryptonote helper functions                                          */
  /************************************************************************/
  size_t get_max_block_size();
  size_t get_max_tx_size();
  bool get_block_reward(size_t median_size, size_t current_block_size, uint64_t already_generated_coins, uint64_t &reward);
  uint8_t get_account_address_checksum(const public_address_outer_blob& bl);
  std::string get_account_address_as_str(const account_public_address& adr);
  bool get_account_address_from_str(uint64_t& prefix, account_public_address& adr, const std::string& str);
  bool get_account_address_from_str(account_public_address& adr, const std::string& str);
  bool is_coinbase(const transaction& tx);

  bool operator ==(const cryptonote::transaction& a, const cryptonote::transaction& b);
  bool operator ==(const cryptonote::block& a, const cryptonote::block& b);
}

template <class T>
std::ostream &print256(std::ostream &o, const T &v) {
  return o << "<" << epee::string_tools::pod_to_hex(v) << ">";
}

bool parse_hash256(const std::string str_hash, crypto::hash& hash);

namespace crypto {
  inline std::ostream &operator <<(std::ostream &o, const crypto::public_key &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::secret_key &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::key_derivation &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::key_image &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::signature &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::hash &v) { return print256(o, v); }
}
