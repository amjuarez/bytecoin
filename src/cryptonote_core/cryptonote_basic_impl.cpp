// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "include_base_utils.h"
using namespace epee;

#include "cryptonote_basic_impl.h"
#include "string_tools.h"
#include "serialization/binary_utils.h"
#include "serialization/vector.h"
#include "cryptonote_format_utils.h"
#include "cryptonote_config.h"
#include "misc_language.h"
#include "common/base58.h"
#include "crypto/hash.h"
#include "common/int-util.h"

namespace cryptonote {

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
    if (!tools::base58::decode_addr(str, prefix, data)) {
      LOG_PRINT_L1("Invalid address format");
      return false;
    }

    if (!::serialization::parse_binary(data, adr)) {
      LOG_PRINT_L1("Account public address keys can't be parsed");
      return false;
    }

    if (!crypto::check_key(adr.m_spendPublicKey) || !crypto::check_key(adr.m_viewPublicKey)) {
      LOG_PRINT_L1("Failed to validate address keys");
      return false;
    }

    return true;
  }
  //-----------------------------------------------------------------------
  bool operator ==(const cryptonote::Transaction& a, const cryptonote::Transaction& b) {
    return cryptonote::get_transaction_hash(a) == cryptonote::get_transaction_hash(b);
  }
  //-----------------------------------------------------------------------
  bool operator ==(const cryptonote::Block& a, const cryptonote::Block& b) {
    return cryptonote::get_block_hash(a) == cryptonote::get_block_hash(b);
  }
}

//--------------------------------------------------------------------------------
bool parse_hash256(const std::string str_hash, crypto::hash& hash)
{
  std::string buf;
  bool res = epee::string_tools::parse_hexstr_to_binbuff(str_hash, buf);
  if (!res || buf.size() != sizeof(crypto::hash))
  {
    std::cout << "invalid hash format: <" << str_hash << '>' << std::endl;
    return false;
  }
  else
  {
    buf.copy(reinterpret_cast<char *>(&hash), sizeof(crypto::hash));
    return true;
  }
}
