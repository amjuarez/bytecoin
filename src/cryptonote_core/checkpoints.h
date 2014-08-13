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
#include <map>
#include "cryptonote_basic_impl.h"


namespace cryptonote
{
  class checkpoints
  {
  public:
    checkpoints();
    bool add_checkpoint(uint64_t height, const std::string& hash_str);
    bool is_in_checkpoint_zone(uint64_t height) const;
    bool check_block(uint64_t height, const crypto::hash& h) const;
    bool check_block(uint64_t height, const crypto::hash& h, bool& is_a_checkpoint) const;
    bool is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height) const;

  private:
    std::map<uint64_t, crypto::hash> m_points;
  };
}
