// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "IWalletLegacy.h"

#include "crypto/hash.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "Wallet/WalletErrors.h"
#include "WalletLegacy/WalletUserTransactionsCache.h"
#include "WalletLegacy/WalletLegacySerialization.h"
#include "WalletLegacy/WalletUtils.h"

#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"
#include <algorithm>

using namespace Crypto;

namespace CryptoNote {

struct LegacyDeposit {
  TransactionId creatingTransactionId;
  TransactionId spendingTransactionId;
  uint32_t term;
  uint64_t amount;
  uint64_t interest;
};

struct LegacyDepositInfo {
  Deposit deposit;
  uint32_t outputInTransaction;
};

void serialize(LegacyDeposit& deposit, ISerializer& serializer) {
  uint64_t creatingTxId = static_cast<uint64_t>(deposit.creatingTransactionId);
  serializer(creatingTxId, "creating_transaction_id");
  deposit.creatingTransactionId = static_cast<size_t>(creatingTxId);

  uint64_t spendingTxIx = static_cast<uint64_t>(deposit.spendingTransactionId);
  serializer(spendingTxIx, "spending_transaction_id");
  deposit.creatingTransactionId = static_cast<size_t>(spendingTxIx);

  serializer(deposit.term, "term");
  serializer(deposit.amount, "amount");
  serializer(deposit.interest, "interest");
}

void serialize(LegacyDepositInfo& depositInfo, ISerializer& serializer) {
  serializer(depositInfo.deposit, "deposit");
  serializer(depositInfo.outputInTransaction, "output_in_transaction");
}

namespace {

class DepositIdSequenceIterator: public std::iterator<std::random_access_iterator_tag, DepositId> {
public:
  explicit DepositIdSequenceIterator(DepositId start) : val(start) {}

  DepositId operator *() const { return val; }

  const DepositIdSequenceIterator& operator ++() { ++val; return *this; }
  DepositIdSequenceIterator operator ++(int) { DepositIdSequenceIterator copy(*this); ++val; return copy; }

  const DepositIdSequenceIterator& operator --() { --val; return *this; }
  DepositIdSequenceIterator operator --(int) { DepositIdSequenceIterator copy(*this); --val; return copy; }

  DepositIdSequenceIterator operator +(const difference_type& n) const { return DepositIdSequenceIterator(val + n); }
  DepositIdSequenceIterator operator -(const difference_type& n) const { return DepositIdSequenceIterator(val - n); }

  difference_type operator -(const DepositIdSequenceIterator& other) const { return val - other.val; }

  DepositIdSequenceIterator& operator +=(const difference_type& n) { val += n; return *this; }
  DepositIdSequenceIterator& operator -=(const difference_type& n) { val -= n; return *this; }

  bool operator <(const DepositIdSequenceIterator& other) const { return val < other.val; }
  bool operator >(const DepositIdSequenceIterator& other) const { return val > other.val; }

  bool operator <=(const DepositIdSequenceIterator& other) const { return !(val > other.val); }
  bool operator >=(const DepositIdSequenceIterator& other) const { return !(val < other.val); }

  bool operator ==(const DepositIdSequenceIterator& other) const { return val == other.val; }
  bool operator !=(const DepositIdSequenceIterator& other) const { return val != other.val; }

private:
  DepositId val;
};

void convertLegacyDeposits(const std::vector<LegacyDepositInfo>& legacyDeposits, UserDeposits& deposits) {
  deposits.reserve(legacyDeposits.size());

  for (const LegacyDepositInfo& legacyDepositInfo: legacyDeposits) {
    DepositInfo info;
    info.deposit.amount = legacyDepositInfo.deposit.amount;
    info.deposit.creatingTransactionId = legacyDepositInfo.deposit.creatingTransactionId;
    info.deposit.interest = legacyDepositInfo.deposit.interest;
    info.deposit.spendingTransactionId = legacyDepositInfo.deposit.spendingTransactionId;
    info.deposit.term = legacyDepositInfo.deposit.term;
    info.deposit.locked = true;
    info.outputInTransaction = legacyDepositInfo.outputInTransaction;

    deposits.push_back(std::move(info));
  }
}

}

WalletUserTransactionsCache::WalletUserTransactionsCache(uint64_t mempoolTxLiveTime) : m_unconfirmedTransactions(mempoolTxLiveTime) {
}

bool WalletUserTransactionsCache::serialize(CryptoNote::ISerializer& s) {
  s(m_transactions, "transactions");
  s(m_transfers, "transfers");
  s(m_unconfirmedTransactions, "unconfirmed");
  s(m_deposits, "deposits");

  if (s.type() == CryptoNote::ISerializer::INPUT) {
    updateUnconfirmedTransactions();
    deleteOutdatedTransactions();
    restoreTransactionOutputToDepositIndex();
    rebuildPaymentsIndex();
  }

  return true;
}

void WalletUserTransactionsCache::deserializeLegacyV1(CryptoNote::ISerializer& s) {
  s(m_transactions, "transactions");
  s(m_transfers, "transfers");
  m_unconfirmedTransactions.deserializeV1(s);

  std::vector<LegacyDepositInfo> legacyDeposits;
  s(legacyDeposits, "deposits");

  convertLegacyDeposits(legacyDeposits, m_deposits);
  restoreTransactionOutputToDepositIndex();
}

bool paymentIdIsSet(const PaymentId& paymentId) {
  return paymentId != NULL_HASH;
}

bool canInsertTransactionToIndex(const WalletLegacyTransaction& info) {
  return info.state == WalletLegacyTransactionState::Active && info.blockHeight != WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT &&
      info.totalAmount > 0 && !info.extra.empty();
}

void WalletUserTransactionsCache::pushToPaymentsIndex(const PaymentId& paymentId, Offset distance) {
  m_paymentsIndex[paymentId].push_back(distance);
}

void WalletUserTransactionsCache::popFromPaymentsIndex(const PaymentId& paymentId, Offset distance) {
  auto it = m_paymentsIndex.find(paymentId);
  if (it == m_paymentsIndex.end()) {
    return;
  }

  auto toErase = std::lower_bound(it->second.begin(), it->second.end(), distance);
  if (toErase == it->second.end() || *toErase != distance) {
    return;
  }

  it->second.erase(toErase);
}

void WalletUserTransactionsCache::rebuildPaymentsIndex() {
  auto begin = std::begin(m_transactions);
  auto end = std::end(m_transactions);
  std::vector<uint8_t> extra;
  for (auto it = begin; it != end; ++it) {
    PaymentId paymentId;
    extra.insert(extra.begin(), it->extra.begin(), it->extra.end());
    if (canInsertTransactionToIndex(*it) && getPaymentIdFromTxExtra(extra, paymentId)) {
      pushToPaymentsIndex(paymentId, std::distance(begin, it));
    }
    extra.clear();
  }
}

uint64_t WalletUserTransactionsCache::unconfirmedTransactionsAmount() const {
  return m_unconfirmedTransactions.countUnconfirmedTransactionsAmount();
}

uint64_t WalletUserTransactionsCache::unconfrimedOutsAmount() const {
  return m_unconfirmedTransactions.countUnconfirmedOutsAmount();
}

uint64_t WalletUserTransactionsCache::countUnconfirmedCreatedDepositsSum() const {
  return m_unconfirmedTransactions.countCreatedDepositsSum();
}

uint64_t WalletUserTransactionsCache::countUnconfirmedSpentDepositsProfit() const {
  return m_unconfirmedTransactions.countSpentDepositsProfit();
}

uint64_t WalletUserTransactionsCache::countUnconfirmedSpentDepositsTotalAmount() const {
  return m_unconfirmedTransactions.countSpentDepositsTotalAmount();
}

size_t WalletUserTransactionsCache::getTransactionCount() const {
  return m_transactions.size();
}

size_t WalletUserTransactionsCache::getTransferCount() const {
  return m_transfers.size();
}

size_t WalletUserTransactionsCache::getDepositCount() const {
  return m_deposits.size();
}

TransactionId WalletUserTransactionsCache::addNewTransaction(uint64_t amount,
                                                             uint64_t fee,
                                                             const std::string& extra,
                                                             const std::vector<WalletLegacyTransfer>& transfers,
                                                             uint64_t unlockTime,
                                                             const std::vector<TransactionMessage>& messages) {
  WalletLegacyTransaction transaction;

  if (!transfers.empty()) {
    transaction.firstTransferId = insertTransfers(transfers);
  } else {
    transaction.firstTransferId = WALLET_LEGACY_INVALID_TRANSFER_ID;
  }

  transaction.transferCount = transfers.size();
  transaction.firstDepositId = WALLET_LEGACY_INVALID_DEPOSIT_ID;
  transaction.depositCount = 0;
  transaction.totalAmount = -static_cast<int64_t>(amount);
  transaction.fee = fee;
  transaction.sentTime = time(nullptr);
  transaction.isCoinbase = false;
  transaction.timestamp = 0;
  transaction.extra = extra;
  transaction.blockHeight = WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT;
  transaction.state = WalletLegacyTransactionState::Sending;
  transaction.unlockTime = unlockTime;

  for (const TransactionMessage& message : messages) {
    transaction.messages.push_back(message.message);
  }

  return insertTransaction(std::move(transaction));
}

void WalletUserTransactionsCache::updateTransaction(
  TransactionId transactionId, const CryptoNote::Transaction& tx, uint64_t amount, const std::vector<TransactionOutputInformation>& usedOutputs) {
  // update extra field from created transaction
  auto& txInfo = m_transactions.at(transactionId);
  txInfo.extra.assign(tx.extra.begin(), tx.extra.end());
  m_unconfirmedTransactions.add(tx, transactionId, amount, usedOutputs);
}

void WalletUserTransactionsCache::updateTransactionSendingState(TransactionId transactionId, std::error_code ec) {
  auto& txInfo = m_transactions.at(transactionId);
  if (ec) {
    txInfo.state = ec.value() == error::TX_CANCELLED ? WalletLegacyTransactionState::Cancelled : WalletLegacyTransactionState::Failed;
    m_unconfirmedTransactions.erase(txInfo.hash);
  } else {
    txInfo.sentTime = time(nullptr); // update sending time
    txInfo.state = WalletLegacyTransactionState::Active;
  }
}

std::deque<std::unique_ptr<WalletLegacyEvent>> WalletUserTransactionsCache::onTransactionUpdated(const TransactionInformation& txInfo,
                                                                                                 int64_t txBalance,
                                                                                                 const std::vector<TransactionOutputInformation>& newDepositOutputs,
                                                                                                 const std::vector<TransactionOutputInformation>& spentDepositOutputs,
                                                                                                 const Currency& currency) {
  std::deque<std::unique_ptr<WalletLegacyEvent>> events;

  TransactionId id = CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID;

  if (!m_unconfirmedTransactions.findTransactionId(txInfo.transactionHash, id)) {
    id = findTransactionByHash(txInfo.transactionHash);
  } else {
    m_unconfirmedTransactions.erase(txInfo.transactionHash);
  }

  bool isCoinbase = txInfo.totalAmountIn == 0;
  uint64_t depositInterest = 0;
  for (const auto& spentDepositOutput : spentDepositOutputs) {
    depositInterest += currency.calculateInterest(spentDepositOutput.amount, spentDepositOutput.term);
  }

  if (id == CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID) {
    WalletLegacyTransaction transaction;
    transaction.firstTransferId = WALLET_LEGACY_INVALID_TRANSFER_ID;
    transaction.transferCount = 0;
    transaction.firstDepositId = WALLET_LEGACY_INVALID_DEPOSIT_ID;
    transaction.depositCount = 0;
    transaction.totalAmount = txBalance;
    transaction.fee = isCoinbase ? 0 : txInfo.totalAmountIn + depositInterest - txInfo.totalAmountOut;
    transaction.sentTime = 0;
    transaction.hash = txInfo.transactionHash;
    transaction.blockHeight = txInfo.blockHeight;
    transaction.isCoinbase = isCoinbase;
    transaction.timestamp = txInfo.timestamp;
    transaction.extra.assign(txInfo.extra.begin(), txInfo.extra.end());
    transaction.state = WalletLegacyTransactionState::Active;
    transaction.unlockTime = txInfo.unlockTime;
    transaction.messages = txInfo.messages;

    id = insertTransaction(std::move(transaction));

    events.push_back(std::unique_ptr<WalletLegacyEvent>(new WalletExternalTransactionCreatedEvent(id)));

    auto updatedDepositIds = createNewDeposits(id, newDepositOutputs, currency);
    if (!updatedDepositIds.empty()) {
      auto& tx = getTransaction(id);
      tx.firstDepositId = updatedDepositIds[0];
      tx.depositCount = updatedDepositIds.size();
    }

    auto spentDepositIds = processSpentDeposits(id, spentDepositOutputs);
    updatedDepositIds.insert(updatedDepositIds.end(), spentDepositIds.begin(), spentDepositIds.end());

    if (!updatedDepositIds.empty()) {
      events.push_back(std::unique_ptr<WalletLegacyEvent>(new WalletDepositsUpdatedEvent(std::move(updatedDepositIds))));
    }
  } else {
    WalletLegacyTransaction& tr = getTransaction(id);
    tr.blockHeight = txInfo.blockHeight;
    tr.timestamp = txInfo.timestamp;
    tr.state = WalletLegacyTransactionState::Active;
    // notification event
    events.push_back(std::unique_ptr<WalletLegacyEvent>(new WalletTransactionUpdatedEvent(id)));

    if (tr.firstDepositId != WALLET_LEGACY_INVALID_DEPOSIT_ID) {
      for (auto id = tr.firstDepositId; id < tr.firstDepositId + tr.depositCount; ++id) {
        m_unconfirmedTransactions.eraseCreatedDeposit(id);
      }
    }
  }

  if (canInsertTransactionToIndex(getTransaction(id)) && paymentIdIsSet(txInfo.paymentId)) {
    pushToPaymentsIndex(txInfo.paymentId, id);
  }

  return events;
}

std::deque<std::unique_ptr<WalletLegacyEvent>> WalletUserTransactionsCache::onTransactionDeleted(const Crypto::Hash& transactionHash) {
  TransactionId id = CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID;
  if (m_unconfirmedTransactions.findTransactionId(transactionHash, id)) {
    m_unconfirmedTransactions.erase(transactionHash);
    // LOG_ERROR("Unconfirmed transaction is deleted: id = " << id << ", hash = " << transactionHash);
    assert(false);
  } else {
    id = findTransactionByHash(transactionHash);
  }

  std::deque<std::unique_ptr<WalletLegacyEvent>> events;
  if (id != CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID) {
    WalletLegacyTransaction& tr = getTransaction(id);
    std::vector<uint8_t> extra(tr.extra.begin(), tr.extra.end());
    PaymentId paymentId;
    if (getPaymentIdFromTxExtra(extra, paymentId)) {
      popFromPaymentsIndex(paymentId, id);
    }

    tr.blockHeight = WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT;
    tr.timestamp = 0;
    tr.state = WalletLegacyTransactionState::Deleted;

    events.push_back(std::unique_ptr<WalletLegacyEvent>(new WalletTransactionUpdatedEvent(id)));

    std::vector<DepositId> unspentDeposits = getDepositIdsBySpendingTransaction(id);

    std::for_each(unspentDeposits.begin(), unspentDeposits.end(), [this] (DepositId id) {
      Deposit& deposit = getDeposit(id);
      deposit.spendingTransactionId = WALLET_LEGACY_INVALID_TRANSACTION_ID;
    });

    DepositIdSequenceIterator depositIdSequenceStart(tr.firstDepositId);
    DepositIdSequenceIterator depositIdSequenceEnd(tr.firstDepositId + tr.depositCount);

    if (depositIdSequenceStart != depositIdSequenceEnd || !unspentDeposits.empty()) {
      unspentDeposits.insert(unspentDeposits.end(), depositIdSequenceStart, depositIdSequenceEnd);
      events.push_back(std::unique_ptr<WalletLegacyEvent>(new WalletDepositsUpdatedEvent(std::move(unspentDeposits))));
    }
  } else {
    // LOG_ERROR("Transaction wasn't found: " << transactionHash);
    assert(false);
  }

  return events;
}

std::vector<Payments> WalletUserTransactionsCache::getTransactionsByPaymentIds(const std::vector<PaymentId>& paymentIds) const {
  std::vector<Payments> payments(paymentIds.size());
  auto payment = payments.begin();
  for (auto& key : paymentIds) {
    payment->paymentId = key;
    auto it = m_paymentsIndex.find(key);
    if (it != m_paymentsIndex.end()) {
      std::transform(it->second.begin(), it->second.end(), std::back_inserter(payment->transactions),
      [this](decltype(it->second)::value_type val) {
        assert(val < m_transactions.size());
        return m_transactions[val];
      });
    }

    ++payment;
  }

  return payments;
}

std::vector<DepositId> WalletUserTransactionsCache::unlockDeposits(const std::vector<TransactionOutputInformation>& transfers) {
  std::vector<DepositId> unlockedDeposits;

  for (const auto& transfer: transfers) {
    auto it = m_transactionOutputToDepositIndex.find(std::tie(transfer.transactionHash, transfer.outputInTransaction));
    if (it == m_transactionOutputToDepositIndex.end()) {
      continue;
    }

    auto id = it->second;
    unlockedDeposits.push_back(id);

    m_deposits[id].deposit.locked = false;
  }

  return unlockedDeposits;
}

std::vector<DepositId> WalletUserTransactionsCache::lockDeposits(const std::vector<TransactionOutputInformation>& transfers) {
  std::vector<DepositId> lockedDeposits;
  for (const auto& transfer: transfers) {
    auto it = m_transactionOutputToDepositIndex.find(std::tie(transfer.transactionHash, transfer.outputInTransaction));
    if (it == m_transactionOutputToDepositIndex.end()) {
      continue;
    }

    auto id = it->second;
    lockedDeposits.push_back(id);

    m_deposits[id].deposit.locked = true;
  }

  return lockedDeposits;
}

TransactionId WalletUserTransactionsCache::findTransactionByTransferId(TransferId transferId) const
{
  TransactionId id;
  for (id = 0; id < m_transactions.size(); ++id) {
    const WalletLegacyTransaction& tx = m_transactions[id];

    if (tx.firstTransferId == WALLET_LEGACY_INVALID_TRANSFER_ID || tx.transferCount == 0)
      continue;

    if (transferId >= tx.firstTransferId && transferId < (tx.firstTransferId + tx.transferCount))
      break;
  }

  if (id == m_transactions.size())
    return WALLET_LEGACY_INVALID_TRANSACTION_ID;

  return id;
}

bool WalletUserTransactionsCache::getTransaction(TransactionId transactionId, WalletLegacyTransaction& transaction) const
{
  if (transactionId >= m_transactions.size())
    return false;

  transaction = m_transactions[transactionId];

  return true;
}

bool WalletUserTransactionsCache::getTransfer(TransferId transferId, WalletLegacyTransfer& transfer) const
{
  if (transferId >= m_transfers.size())
    return false;

  transfer = m_transfers[transferId];

  return true;
}

bool WalletUserTransactionsCache::getDeposit(DepositId depositId, Deposit& deposit) const {
  if (depositId >= m_deposits.size()) {
    return false;
  }

  deposit = m_deposits[depositId].deposit;
  return true;
}

Deposit& WalletUserTransactionsCache::getDeposit(DepositId depositId) {
  assert(depositId < m_deposits.size());

  return m_deposits[depositId].deposit;
}

TransactionId WalletUserTransactionsCache::insertTransaction(WalletLegacyTransaction&& Transaction) {
  m_transactions.emplace_back(std::move(Transaction));
  return m_transactions.size() - 1;
}

TransactionId WalletUserTransactionsCache::findTransactionByHash(const Hash& hash) {
  auto it = std::find_if(m_transactions.begin(), m_transactions.end(), [&hash](const WalletLegacyTransaction& tx) { return tx.hash == hash; });

  if (it == m_transactions.end())
    return CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID;

  return std::distance(m_transactions.begin(), it);
}

bool WalletUserTransactionsCache::isUsed(const TransactionOutputInformation& out) const {
  return m_unconfirmedTransactions.isUsed(out);
}

WalletLegacyTransaction& WalletUserTransactionsCache::getTransaction(TransactionId transactionId) {
  return m_transactions.at(transactionId);
}

TransferId WalletUserTransactionsCache::insertTransfers(const std::vector<WalletLegacyTransfer>& transfers) {
  std::copy(transfers.begin(), transfers.end(), std::back_inserter(m_transfers));
  return m_transfers.size() - transfers.size();
}

void WalletUserTransactionsCache::updateUnconfirmedTransactions() {
  for (TransactionId id = 0; id < m_transactions.size(); ++id) {
    if (m_transactions[id].blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
      m_unconfirmedTransactions.updateTransactionId(m_transactions[id].hash, id);
    }
  }
}

WalletLegacyTransfer& WalletUserTransactionsCache::getTransfer(TransferId transferId) {
  return m_transfers.at(transferId);
}
  
void WalletUserTransactionsCache::reset() {
  m_transactions.clear();
  m_transfers.clear();
  m_unconfirmedTransactions.reset();
}

std::vector<TransactionId> WalletUserTransactionsCache::deleteOutdatedTransactions() {
  auto deletedTransactions = m_unconfirmedTransactions.deleteOutdatedTransactions();

  for (auto id: deletedTransactions) {
    assert(id < m_transactions.size());
    m_transactions[id].state = WalletLegacyTransactionState::Deleted;
  }

  return deletedTransactions;
}

void WalletUserTransactionsCache::restoreTransactionOutputToDepositIndex() {
  m_transactionOutputToDepositIndex.clear();

  DepositId id = 0;
  for (const auto& d: m_deposits) {
    WalletLegacyTransaction transaction = m_transactions[d.deposit.creatingTransactionId];
    m_transactionOutputToDepositIndex[std::tie(transaction.hash, d.outputInTransaction)] = id;
    ++id;
  }
}

DepositId WalletUserTransactionsCache::insertDeposit(const Deposit& deposit, size_t depositIndexInTransaction, const Hash& transactionHash) {
  DepositInfo info;
  info.deposit = deposit;
  info.outputInTransaction = static_cast<uint32_t>(depositIndexInTransaction);

  DepositId id = m_deposits.size();
  m_deposits.push_back(std::move(info));

  m_transactionOutputToDepositIndex.emplace(std::piecewise_construct, std::forward_as_tuple(transactionHash, static_cast<uint32_t>(depositIndexInTransaction)),
    std::forward_as_tuple(id));

  return id;
}

bool WalletUserTransactionsCache::getDepositInTransactionInfo(DepositId depositId, Hash& transactionHash, uint32_t& outputInTransaction) {
  if (depositId >= m_deposits.size()) {
    return false;
  }

  assert(m_deposits[depositId].deposit.creatingTransactionId < m_transactions.size());

  outputInTransaction = m_deposits[depositId].outputInTransaction;
  transactionHash = m_transactions[m_deposits[depositId].deposit.creatingTransactionId].hash;

  return true;
}

std::vector<DepositId> WalletUserTransactionsCache::createNewDeposits(TransactionId creatingTransactionId, const std::vector<TransactionOutputInformation>& depositOutputs,
    const Currency& currency) {
  std::vector<DepositId> deposits;

  for (size_t i = 0; i < depositOutputs.size(); i++) {
    auto id = insertNewDeposit(depositOutputs[i], creatingTransactionId, currency);
    deposits.push_back(id);
  }
  return deposits;
}

DepositId WalletUserTransactionsCache::insertNewDeposit(const TransactionOutputInformation& depositOutput, TransactionId creatingTransactionId,
  const Currency& currency) {
  assert(depositOutput.type == TransactionTypes::OutputType::Multisignature);
  assert(depositOutput.term != 0);
  assert(m_transactionOutputToDepositIndex.find(std::tie(depositOutput.transactionHash, depositOutput.outputInTransaction)) == m_transactionOutputToDepositIndex.end());

  Deposit deposit;
  deposit.amount = depositOutput.amount;
  deposit.creatingTransactionId = creatingTransactionId;
  deposit.term = depositOutput.term;
  deposit.spendingTransactionId = WALLET_LEGACY_INVALID_TRANSACTION_ID;
  deposit.interest = currency.calculateInterest(deposit.amount, deposit.term);
  deposit.locked = true;

  return insertDeposit(deposit, depositOutput.outputInTransaction, depositOutput.transactionHash);
}

std::vector<DepositId> WalletUserTransactionsCache::processSpentDeposits(TransactionId spendingTransactionId, const std::vector<TransactionOutputInformation>& spentDepositOutputs) {
  std::vector<DepositId> deposits;
  deposits.reserve(spentDepositOutputs.size());

  for (size_t i = 0; i < spentDepositOutputs.size(); i++) {
    auto depositId = getDepositId(spentDepositOutputs[i].transactionHash, spentDepositOutputs[i].outputInTransaction);
    assert(depositId != WALLET_LEGACY_INVALID_DEPOSIT_ID);
    if (depositId == WALLET_LEGACY_INVALID_DEPOSIT_ID) {
      throw std::invalid_argument("processSpentDeposits error: requested deposit doesn't exist");
    }

    auto& d = m_deposits[depositId];
    d.deposit.spendingTransactionId = spendingTransactionId;
    deposits.push_back(depositId);
  }
  return deposits;
}

DepositId WalletUserTransactionsCache::getDepositId(const Hash& creatingTransactionHash, uint32_t outputInTransaction) {
  auto it = m_transactionOutputToDepositIndex.find(std::tie(creatingTransactionHash, outputInTransaction));
  if (it == m_transactionOutputToDepositIndex.end()) {
    return WALLET_LEGACY_INVALID_DEPOSIT_ID;
  }

  return it->second;
}

std::vector<DepositId> WalletUserTransactionsCache::getDepositIdsBySpendingTransaction(TransactionId transactionId) {
  std::vector<DepositId> ids;

  for (DepositId dId = 0; dId < m_deposits.size(); ++dId) {
    auto& deposit = m_deposits[dId].deposit;

    if (deposit.spendingTransactionId == transactionId) {
      ids.push_back(dId);
    }
  }

  return ids;
}

void WalletUserTransactionsCache::addCreatedDeposit(DepositId id, uint64_t totalAmount) {
  m_unconfirmedTransactions.addCreatedDeposit(id, totalAmount);
}

void WalletUserTransactionsCache::addDepositSpendingTransaction(const Hash& transactionHash, const UnconfirmedSpentDepositDetails& details) {
  m_unconfirmedTransactions.addDepositSpendingTransaction(transactionHash, details);
}

void WalletUserTransactionsCache::eraseCreatedDeposit(DepositId id) {
  m_unconfirmedTransactions.eraseCreatedDeposit(id);
}

} //namespace CryptoNote
