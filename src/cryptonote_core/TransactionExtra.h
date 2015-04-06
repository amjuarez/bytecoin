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