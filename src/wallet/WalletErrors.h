// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <system_error>

namespace cryptonote {
namespace error {

// custom error conditions enum type:
enum WalletErrorCodes {
  NOT_INITIALIZED = 1,
  ALREADY_INITIALIZED,
  WRONG_STATE,
  WRONG_PASSWORD,
  INTERNAL_WALLET_ERROR,
  MIXIN_COUNT_TOO_BIG,
  BAD_ADDRESS,
  TRANSACTION_SIZE_TOO_BIG,
  WRONG_AMOUNT,
  SUM_OVERFLOW,
  ZERO_DESTINATION,
  TX_CANCEL_IMPOSSIBLE,
  TX_CANCELLED,
  OPERATION_CANCELLED
};

// custom category:
class WalletErrorCategory : public std::error_category {
public:
  static WalletErrorCategory INSTANCE;

  virtual const char* name() const throw() {
    return "WalletErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const {
    switch (ev) {
    case NOT_INITIALIZED:     return "Object was not initialized";
    case WRONG_PASSWORD:      return "The password is wrong";
    case ALREADY_INITIALIZED: return "The object is already initialized";
    case INTERNAL_WALLET_ERROR: return "Internal error occured";
    case MIXIN_COUNT_TOO_BIG: return "MixIn count is too big";
    case BAD_ADDRESS:         return "Bad address";
    case TRANSACTION_SIZE_TOO_BIG: return "Transaction size is too big";
    case WRONG_AMOUNT:        return "Wrong amount";
    case SUM_OVERFLOW:        return "Sum overflow";
    case ZERO_DESTINATION:    return "The destination is empty";
    case TX_CANCEL_IMPOSSIBLE: return "Impossible to cancel transaction";
    case WRONG_STATE:         return "The wallet is in wrong state (maybe loading or saving), try again later";
    case OPERATION_CANCELLED: return "The operation you've requested has been cancelled";
    default:                  return "Unknown error";
    }
  }

private:
  WalletErrorCategory() {
  }
};

}
}

inline std::error_code make_error_code(cryptonote::error::WalletErrorCodes e) {
  return std::error_code(static_cast<int>(e), cryptonote::error::WalletErrorCategory::INSTANCE);
}
