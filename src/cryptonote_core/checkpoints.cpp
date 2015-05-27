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

#include "checkpoints.h"
#include "Common/StringTools.h"

using namespace Logging;

namespace CryptoNote {
//---------------------------------------------------------------------------
checkpoints::checkpoints(Logging::ILogger &log) : logger(log, "checkpoints") {}
//---------------------------------------------------------------------------
bool checkpoints::add_checkpoint(uint64_t height, const std::string &hash_str) {
  crypto::hash h = null_hash;

  if (!Common::podFromHex(hash_str, h)) {
    logger(ERROR) << "WRONG HASH IN CHECKPOINTS!!!";
    return false;
  }

  if (!(0 == m_points.count(height))) {
    logger(ERROR) << "WRONG HASH IN CHECKPOINTS!!!";
    return false;
  }

  m_points[height] = h;
  return true;
}
//---------------------------------------------------------------------------
bool checkpoints::is_in_checkpoint_zone(uint64_t height) const {
  return !m_points.empty() && (height <= (--m_points.end())->first);
}
//---------------------------------------------------------------------------
bool checkpoints::check_block(uint64_t height, const crypto::hash &h,
                              bool &is_a_checkpoint) const {
  auto it = m_points.find(height);
  is_a_checkpoint = it != m_points.end();
  if (!is_a_checkpoint)
    return true;

  if (it->second == h) {
    logger(Logging::INFO, Logging::GREEN) 
      << "CHECKPOINT PASSED FOR HEIGHT " << height << " " << h;
    return true;
  } else {
    logger(Logging::ERROR) << "CHECKPOINT FAILED FOR HEIGHT " << height
                           << ". EXPECTED HASH: " << it->second
                           << ", FETCHED HASH: " << h;
    return false;
  }
}
//---------------------------------------------------------------------------
bool checkpoints::check_block(uint64_t height, const crypto::hash &h) const {
  bool ignored;
  return check_block(height, h, ignored);
}
//---------------------------------------------------------------------------
bool checkpoints::is_alternative_block_allowed(uint64_t blockchain_height,
                                               uint64_t block_height) const {
  if (0 == block_height)
    return false;

  auto it = m_points.upper_bound(blockchain_height);
  // Is blockchain_height before the first checkpoint?
  if (it == m_points.begin())
    return true;

  --it;
  uint64_t checkpoint_height = it->first;
  return checkpoint_height < block_height;
}
}
