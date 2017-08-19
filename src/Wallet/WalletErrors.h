// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <system_error>

namespace CryptoNote {
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
  OPERATION_CANCELLED,
  TX_TRANSFER_IMPOSSIBLE,
  WRONG_VERSION,
  FEE_TOO_SMALL,
  KEY_GENERATION_ERROR,
  INDEX_OUT_OF_RANGE,
  ADDRESS_ALREADY_EXISTS,
  TRACKING_MODE,
  WRONG_PARAMETERS,
  OBJECT_NOT_FOUND,
  WALLET_NOT_FOUND,
  CHANGE_ADDRESS_REQUIRED,
  CHANGE_ADDRESS_NOT_FOUND
};

// custom category:
class WalletErrorCategory : public std::error_category {
public:
  static WalletErrorCategory INSTANCE;

  virtual const char* name() const throw() override {
    return "WalletErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() override {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const override {
    switch (ev) {
    case NOT_INITIALIZED:          return "Object was not initialized";
    case WRONG_PASSWORD:           return "The password is wrong";
    case ALREADY_INITIALIZED:      return "The object is already initialized";
    case INTERNAL_WALLET_ERROR:    return "Internal error occurred";
    case MIXIN_COUNT_TOO_BIG:      return "MixIn count is too big";
    case BAD_ADDRESS:              return "Bad address";
    case TRANSACTION_SIZE_TOO_BIG: return "Transaction size is too big";
    case WRONG_AMOUNT:             return "Wrong amount";
    case SUM_OVERFLOW:             return "Sum overflow";
    case ZERO_DESTINATION:         return "The destination is empty";
    case TX_CANCEL_IMPOSSIBLE:     return "Impossible to cancel transaction";
    case WRONG_STATE:              return "The wallet is in wrong state (maybe loading or saving), try again later";
    case OPERATION_CANCELLED:      return "The operation you've requested has been cancelled";
    case TX_TRANSFER_IMPOSSIBLE:   return "Transaction transfer impossible";
    case WRONG_VERSION:            return "Wrong version";
    case FEE_TOO_SMALL:            return "Transaction fee is too small";
    case KEY_GENERATION_ERROR:     return "Cannot generate new key";
    case INDEX_OUT_OF_RANGE:       return "Index is out of range";
    case ADDRESS_ALREADY_EXISTS:   return "Address already exists";
    case TRACKING_MODE:            return "The wallet is in tracking mode";
    case WRONG_PARAMETERS:         return "Wrong parameters passed";
    case OBJECT_NOT_FOUND:         return "Object not found";
    case WALLET_NOT_FOUND:         return "Requested wallet not found";
    case CHANGE_ADDRESS_REQUIRED:  return "Change address required";
    case CHANGE_ADDRESS_NOT_FOUND: return "Change address not found";
    default:                       return "Unknown error";
    }
  }

private:
  WalletErrorCategory() {
  }
};

}
}

inline std::error_code make_error_code(CryptoNote::error::WalletErrorCodes e) {
  return std::error_code(static_cast<int>(e), CryptoNote::error::WalletErrorCategory::INSTANCE);
}

namespace std {

template <>
struct is_error_code_enum<CryptoNote::error::WalletErrorCodes>: public true_type {};

}
