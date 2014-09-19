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

#include <unordered_map>

#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/Currency.h"
#include "IWallet.h"

namespace CryptoNote {

struct TransferDetails
{
  uint64_t blockHeight;
  cryptonote::Transaction tx;
  size_t internalOutputIndex;
  uint64_t globalOutputIndex;
  bool spent;
  crypto::key_image keyImage;

  uint64_t amount() const
  {
    return tx.vout[internalOutputIndex].amount;
  }
};

class WalletTransferDetails
{
public:
  WalletTransferDetails(const cryptonote::Currency& currency, const std::vector<crypto::hash>& blockchain);
  ~WalletTransferDetails();

  TransferDetails& getTransferDetails(size_t idx);
  void addTransferDetails(const TransferDetails& details);
  bool getTransferDetailsIdxByKeyImage(const crypto::key_image& image, size_t& idx);

  uint64_t countActualBalance() const;
  uint64_t countPendingBalance() const;
  bool isTransferUnlocked(const TransferDetails& td) const;

  uint64_t selectTransfersToSend(uint64_t neededMoney, bool addDust, uint64_t dust, std::list<size_t>& selectedTransfers);

  void detachTransferDetails(size_t height);

  template <typename Archive>
  void save(Archive& ar, bool saveCache) const;

  template<typename Archive>
  void load(Archive& ar);

private:
  bool isTxSpendtimeUnlocked(uint64_t unlock_time) const;

  typedef std::vector<TransferDetails> TransferContainer;
  TransferContainer m_transfers;

  typedef std::unordered_map<crypto::key_image, size_t> KeyImagesContainer;
  KeyImagesContainer m_keyImages;

  const cryptonote::Currency& m_currency;
  const std::vector<crypto::hash>& m_blockchain;
};

template <typename Archive>
void WalletTransferDetails::save(Archive& ar, bool saveCache) const
{
  const TransferContainer& transfers = saveCache ? m_transfers : TransferContainer();
  const KeyImagesContainer& keyImages = saveCache ? m_keyImages : KeyImagesContainer();

  ar << transfers;
  ar << keyImages;
}

template<typename Archive>
void WalletTransferDetails::load(Archive& ar)
{
  ar >> m_transfers;
  ar >> m_keyImages;
}

} //namespace CryptoNote
