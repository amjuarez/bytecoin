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

#include "WalletTransferDetails.h"

#include <utility>
#include <chrono>
#include <random>

#include <crypto/crypto.h>

#include "WalletErrors.h"

#define DEFAULT_TX_SPENDABLE_AGE 10

namespace {

template<typename URNG, typename T>
T popRandomValue(URNG& randomGenerator, std::vector<T>& vec) {
  CHECK_AND_ASSERT_MES(!vec.empty(), T(), "Vector must be non-empty");

  std::uniform_int_distribution<size_t> distribution(0, vec.size() - 1);
  size_t idx = distribution(randomGenerator);

  T res = vec[idx];
  if (idx + 1 != vec.size()) {
    vec[idx] = vec.back();
  }
  vec.resize(vec.size() - 1);

  return res;
}

}

namespace CryptoNote
{

WalletTransferDetails::WalletTransferDetails(const cryptonote::Currency& currency, const std::vector<crypto::hash>& blockchain) :
  m_currency(currency), m_blockchain(blockchain) {
}

WalletTransferDetails::~WalletTransferDetails() {
}

TransferDetails& WalletTransferDetails::getTransferDetails(size_t idx) {
  return m_transfers.at(idx);
}

void WalletTransferDetails::addTransferDetails(const TransferDetails& details) {
  m_transfers.push_back(details);
  size_t idx = m_transfers.size() - 1;

  m_keyImages.insert(std::make_pair(details.keyImage, idx));
}

bool WalletTransferDetails::getTransferDetailsIdxByKeyImage(const crypto::key_image& image, size_t& idx) {
  auto it = m_keyImages.find(image);
  if (it == m_keyImages.end())
    return false;

  idx = it->second;
  return true;
}

bool WalletTransferDetails::isTxSpendtimeUnlocked(uint64_t unlockTime) const {
  if (unlockTime < m_currency.maxBlockHeight()) {
    // interpret as block index
    return m_blockchain.size()-1 + m_currency.lockedTxAllowedDeltaBlocks() >= unlockTime;
  } else {
    // interpret as time
    uint64_t current_time = static_cast<uint64_t>(time(NULL));
    return current_time + m_currency.lockedTxAllowedDeltaSeconds() >= unlockTime;
  }
  return false;
}

bool WalletTransferDetails::isTransferUnlocked(const TransferDetails& td) const
{
  if(!isTxSpendtimeUnlocked(td.tx.unlockTime))
    return false;

  if(td.blockHeight + DEFAULT_TX_SPENDABLE_AGE > m_blockchain.size())
    return false;

  return true;
}

uint64_t WalletTransferDetails::countActualBalance() const
{
  uint64_t amount = 0;
  for (auto& transfer: m_transfers) {
    if(!transfer.spent && isTransferUnlocked(transfer))
      amount += transfer.amount();
  }

  return amount;
}

uint64_t WalletTransferDetails::countPendingBalance() const
{
  uint64_t amount = 0;
  for (auto& td: m_transfers) {
    if (!td.spent)
      amount += td.amount();
  }

  return amount;
}

uint64_t WalletTransferDetails::selectTransfersToSend(uint64_t neededMoney, bool addDust, uint64_t dust, std::list<crypto::key_image>& selectedTransfers) {
  std::vector<size_t> unusedTransfers;
  std::vector<size_t> unusedDust;

  for (size_t i = 0; i < m_transfers.size(); ++i) {
    const TransferDetails& td = m_transfers[i];
    if (!td.spent && isTransferUnlocked(td)) {
      if (dust < td.amount())
        unusedTransfers.push_back(i);
      else
        unusedDust.push_back(i);
    }
  }

  std::default_random_engine randomGenerator(crypto::rand<std::default_random_engine::result_type>());
  bool selectOneDust = addDust && !unusedDust.empty();
  uint64_t foundMoney = 0;
  while (foundMoney < neededMoney && (!unusedTransfers.empty() || !unusedDust.empty())) {
    size_t idx;
    if (selectOneDust) {
      idx = popRandomValue(randomGenerator, unusedDust);
      selectOneDust = false;
    }
    else
    {
      idx = !unusedTransfers.empty() ? popRandomValue(randomGenerator, unusedTransfers) : popRandomValue(randomGenerator, unusedDust);
    }

    selectedTransfers.push_back(m_transfers[idx].keyImage);
    foundMoney += m_transfers[idx].amount();
  }

  return foundMoney;
}

void WalletTransferDetails::detachTransferDetails(size_t height) {
  auto it = std::find_if(m_transfers.begin(), m_transfers.end(), [&](const TransferDetails& td){return td.blockHeight >= height;});
  size_t start = it - m_transfers.begin();

  for(size_t i = start; i!= m_transfers.size();i++) {
    auto ki = m_keyImages.find(m_transfers[i].keyImage);
    if(ki == m_keyImages.end()) throw std::system_error(make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR));

    m_keyImages.erase(ki);
  }
  m_transfers.erase(it, m_transfers.end());
}

} /* namespace CryptoNote */
