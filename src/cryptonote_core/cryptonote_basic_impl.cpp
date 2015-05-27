// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include "cryptonote_basic_impl.h"
#include "serialization/binary_utils.h"
#include "serialization/vector.h"
#include "cryptonote_format_utils.h"
#include "Common/base58.h"
#include "crypto/hash.h"
#include "Common/int-util.h"

namespace CryptoNote {

  /************************************************************************/
  /* Cryptonote helper functions                                          */
  /************************************************************************/
  //-----------------------------------------------------------------------------------------------
  uint64_t getPenalizedAmount(uint64_t amount, size_t medianSize, size_t currentBlockSize) {
    static_assert(sizeof(size_t) >= sizeof(uint32_t), "size_t is too small");
    assert(currentBlockSize <= 2 * medianSize);
    assert(medianSize <= std::numeric_limits<uint32_t>::max());
    assert(currentBlockSize <= std::numeric_limits<uint32_t>::max());

    if (amount == 0) {
      return 0;
    }

    if (currentBlockSize <= medianSize) {
      return amount;
    }

    uint64_t productHi;
    uint64_t productLo = mul128(amount, currentBlockSize * (UINT64_C(2) * medianSize - currentBlockSize), &productHi);

    uint64_t penalizedAmountHi;
    uint64_t penalizedAmountLo;
    div128_32(productHi, productLo, static_cast<uint32_t>(medianSize), &penalizedAmountHi, &penalizedAmountLo);
    div128_32(penalizedAmountHi, penalizedAmountLo, static_cast<uint32_t>(medianSize), &penalizedAmountHi, &penalizedAmountLo);

    assert(0 == penalizedAmountHi);
    assert(penalizedAmountLo < amount);

    return penalizedAmountLo;
  }
  //-----------------------------------------------------------------------
  std::string getAccountAddressAsStr(uint64_t prefix, const AccountPublicAddress& adr) {
    blobdata blob;
    bool r = t_serializable_object_to_blob(adr, blob);
    assert(r);
    return tools::base58::encode_addr(prefix, blob);
  }
  //-----------------------------------------------------------------------
  bool is_coinbase(const Transaction& tx) {
    if(tx.vin.size() != 1) {
      return false;
    }

    if(tx.vin[0].type() != typeid(TransactionInputGenerate)) {
      return false;
    }

    return true;
  }
  //-----------------------------------------------------------------------
  bool parseAccountAddressString(uint64_t& prefix, AccountPublicAddress& adr, const std::string& str) {
    blobdata data;

    return
      tools::base58::decode_addr(str, prefix, data) &&
      ::serialization::parse_binary(data, adr) &&
      crypto::check_key(adr.m_spendPublicKey) &&
      crypto::check_key(adr.m_viewPublicKey);
  }
  //-----------------------------------------------------------------------
  bool operator ==(const CryptoNote::Transaction& a, const CryptoNote::Transaction& b) {
    return CryptoNote::get_transaction_hash(a) == CryptoNote::get_transaction_hash(b);
  }
  //-----------------------------------------------------------------------
  bool operator ==(const CryptoNote::Block& a, const CryptoNote::Block& b) {
    return CryptoNote::get_block_hash(a) == CryptoNote::get_block_hash(b);
  }
}

//--------------------------------------------------------------------------------
bool parse_hash256(const std::string& str_hash, crypto::hash& hash) {
  if (!Common::podFromHex(str_hash, hash)) {
    std::cout << "invalid hash format: <" << str_hash << '>' << std::endl;
    return false;
  }
  return true;
}
