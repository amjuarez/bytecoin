// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <system_error>

namespace CryptoNote {
namespace error {

enum InProcessNodeErrorCodes {
  NOT_INITIALIZED = 1,
  ALREADY_INITIALIZED,
  NETWORK_ERROR,
  NODE_BUSY,
  INTERNAL_NODE_ERROR,
  REQUEST_ERROR
};

class InProcessNodeErrorCategory : public std::error_category {
public:
  static InProcessNodeErrorCategory INSTANCE;

  virtual const char* name() const throw() override {
    return "InProcessNodeErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() override {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const override {
    switch (ev) {
      case NOT_INITIALIZED:     return "Object was not initialized";
      case ALREADY_INITIALIZED: return "Object has been already initialized";
      case NETWORK_ERROR:       return "Network error";
      case NODE_BUSY:           return "Node is busy";
      case INTERNAL_NODE_ERROR: return "Internal node error";
      case REQUEST_ERROR:       return "Error in request parameters";
      default:                  return "Unknown error";
    }
  }

private:
  InProcessNodeErrorCategory() {
  }
};

} //namespace error
} //namespace CryptoNote

inline std::error_code make_error_code(CryptoNote::error::InProcessNodeErrorCodes e) {
  return std::error_code(static_cast<int>(e), CryptoNote::error::InProcessNodeErrorCategory::INSTANCE);
}
