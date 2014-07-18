// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <unordered_map>

#include "cryptonote_core/cryptonote_format_utils.h"
#include "IWallet.h"

namespace CryptoNote {

struct TransferDetails
{
  uint64_t blockHeight;
  cryptonote::transaction tx;
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
  WalletTransferDetails(const std::vector<crypto::hash>& blockchain);
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
