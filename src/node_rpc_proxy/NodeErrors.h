// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <system_error>

namespace cryptonote {
namespace error {

// custom error conditions enum type:
enum NodeErrorCodes {
  NOT_INITIALIZED = 1,
  ALREADY_INITIALIZED,
  NETWORK_ERROR,
  NODE_BUSY,
  INTERNAL_NODE_ERROR,
};

// custom category:
class NodeErrorCategory : public std::error_category {
public:
  static NodeErrorCategory INSTANCE;

  virtual const char* name() const throw() {
    return "NodeErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const {
    switch (ev) {
    case NOT_INITIALIZED:     return "Object was not initialized";
    case ALREADY_INITIALIZED: return "Object has been already initialized";
    case NETWORK_ERROR:       return "Network error";
    case NODE_BUSY:           return "Node is busy";
    case INTERNAL_NODE_ERROR: return "Internal node error";
    default:                  return "Unknown error";
    }
  }

private:
  NodeErrorCategory() {
  }
};

}
}
 
inline std::error_code make_error_code(cryptonote::error::NodeErrorCodes e) {
  return std::error_code(static_cast<int>(e), cryptonote::error::NodeErrorCategory::INSTANCE);
}
