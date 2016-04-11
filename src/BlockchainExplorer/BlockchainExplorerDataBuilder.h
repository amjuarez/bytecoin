// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include <vector>
#include <array>

#include "CryptoNoteProtocol/ICryptoNoteProtocolQuery.h"
#include "CryptoNoteCore/ICore.h"
#include "BlockchainExplorerData.h"

namespace CryptoNote {

class BlockchainExplorerDataBuilder
{
public:
  BlockchainExplorerDataBuilder(CryptoNote::ICore& core, CryptoNote::ICryptoNoteProtocolQuery& protocol);

  BlockchainExplorerDataBuilder(const BlockchainExplorerDataBuilder&) = delete;
  BlockchainExplorerDataBuilder(BlockchainExplorerDataBuilder&&) = delete;

  BlockchainExplorerDataBuilder& operator=(const BlockchainExplorerDataBuilder&) = delete;
  BlockchainExplorerDataBuilder& operator=(BlockchainExplorerDataBuilder&&) = delete;

  bool fillBlockDetails(const Block& block, BlockDetails& blockDetails);
  bool fillTransactionDetails(const Transaction &tx, TransactionDetails& txRpcInfo, uint64_t timestamp = 0);

  static bool getPaymentId(const Transaction& transaction, Crypto::Hash& paymentId);

private:
  bool getMixin(const Transaction& transaction, uint64_t& mixin);
  bool fillTxExtra(const std::vector<uint8_t>& rawExtra, TransactionExtraDetails& extraDetails);
  size_t median(std::vector<size_t>& v);

  CryptoNote::ICore& core;
  CryptoNote::ICryptoNoteProtocolQuery& protocol;
};
}
