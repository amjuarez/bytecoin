// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_format_utils.h"

namespace CryptoNote {

  inline std::vector<uint8_t> stringToVector(const std::string& s) {
    std::vector<uint8_t> vec(
      reinterpret_cast<const uint8_t*>(s.data()),
      reinterpret_cast<const uint8_t*>(s.data()) + s.size());
    return vec;
  }

  class TransactionExtra {
  public:
    TransactionExtra() {}
    TransactionExtra(const std::vector<uint8_t>& extra) {
      parse(extra);        
    }

    bool parse(const std::vector<uint8_t>& extra) {
      fields.clear();
      return cryptonote::parse_tx_extra(extra, fields);
    }

    template <typename T>
    bool get(T& value) const {
      auto it = find(typeid(T));
      if (it == fields.end()) {
        return false;
      }
      value = boost::get<T>(*it);
      return true;
    }

    template <typename T>
    void set(const T& value) {
      auto it = find(typeid(T));
      if (it != fields.end()) {
        *it = value;
      } else {
        fields.push_back(value);
      }
    }

    bool getPublicKey(crypto::public_key& pk) const {
      cryptonote::tx_extra_pub_key extraPk;
      if (!get(extraPk)) {
        return false;
      }
      pk = extraPk.pub_key;
      return true;
    }

    std::vector<uint8_t> serialize() const {
      std::ostringstream out;
      binary_archive<true> ar(out);
      for (const auto& f : fields) {
        ::do_serialize(ar, const_cast<cryptonote::tx_extra_field&>(f));
      }
      return stringToVector(out.str());
    }

  private:

    std::vector<cryptonote::tx_extra_field>::const_iterator find(const std::type_info& t) const {
      return std::find_if(fields.begin(), fields.end(), [&t](const cryptonote::tx_extra_field& f) { return t == f.type(); });
    }

    std::vector<cryptonote::tx_extra_field>::iterator find(const std::type_info& t) {
      return std::find_if(fields.begin(), fields.end(), [&t](const cryptonote::tx_extra_field& f) { return t == f.type(); });
    }

    std::vector<cryptonote::tx_extra_field> fields;
  };

}