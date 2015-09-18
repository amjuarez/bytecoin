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

#include "WalletGreen.h"

#include <algorithm>
#include <ctime>
#include <cassert>
#include <numeric>
#include <random>
#include <set>
#include <tuple>
#include <utility>
#include <System/EventLock.h>

#include "ITransaction.h"

#include "Common/ShuffleGenerator.h"
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "crypto/crypto.h"
#include "Transfers/TransfersContainer.h"
#include "WalletSerialization.h"
#include "WalletErrors.h"

using namespace Common;
using namespace Crypto;
using namespace CryptoNote;

namespace {

void asyncRequestCompletion(System::Event& requestFinished) {
  requestFinished.set();
}

void parseAddressString(const std::string& string, const CryptoNote::Currency& currency, CryptoNote::AccountPublicAddress& address) {
  if (!currency.parseAccountAddressString(string, address)) {
    throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS));
  }
}

bool validateAddress(const std::string& address, const CryptoNote::Currency& currency) {
  CryptoNote::AccountPublicAddress ignore;
  return currency.parseAccountAddressString(address, ignore);
}

void validateAddresses(const std::vector<CryptoNote::WalletTransfer>& destinations, const CryptoNote::Currency& currency) {
  for (const auto& destination: destinations) {
    if (!validateAddress(destination.address, currency)) {
      throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS));
    }
  }
}

uint64_t countNeededMoney(const std::vector<CryptoNote::WalletTransfer>& destinations, uint64_t fee) {
  uint64_t neededMoney = 0;
  for (const auto& transfer: destinations) {
    if (transfer.amount == 0) {
      throw std::system_error(make_error_code(CryptoNote::error::ZERO_DESTINATION));
    } else if (transfer.amount < 0) {
      throw std::system_error(make_error_code(std::errc::invalid_argument));
    }

    //to supress warning
    uint64_t uamount = static_cast<uint64_t>(transfer.amount);
    neededMoney += uamount;
    if (neededMoney < uamount) {
      throw std::system_error(make_error_code(CryptoNote::error::SUM_OVERFLOW));
    }
  }

  neededMoney += fee;
  if (neededMoney < fee) {
    throw std::system_error(make_error_code(CryptoNote::error::SUM_OVERFLOW));
  }

  return neededMoney;
}

void checkIfEnoughMixins(std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult, uint64_t mixIn) {
  auto notEnoughIt = std::find_if(mixinResult.begin(), mixinResult.end(),
    [mixIn] (const CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& ofa) { return ofa.outs.size() < mixIn; } );

  if (mixIn == 0 && mixinResult.empty()) {
    throw std::system_error(make_error_code(CryptoNote::error::MIXIN_COUNT_TOO_BIG));
  }

  if (notEnoughIt != mixinResult.end()) {
    throw std::system_error(make_error_code(CryptoNote::error::MIXIN_COUNT_TOO_BIG));
  }
}

CryptoNote::WalletEvent makeTransactionUpdatedEvent(size_t id) {
  CryptoNote::WalletEvent event;
  event.type = CryptoNote::WalletEventType::TRANSACTION_UPDATED;
  event.transactionUpdated.transactionIndex = id;

  return event;
}

CryptoNote::WalletEvent makeTransactionCreatedEvent(size_t id) {
  CryptoNote::WalletEvent event;
  event.type = CryptoNote::WalletEventType::TRANSACTION_CREATED;
  event.transactionCreated.transactionIndex = id;

  return event;
}

CryptoNote::WalletEvent makeMoneyUnlockedEvent() {
  CryptoNote::WalletEvent event;
  event.type = CryptoNote::WalletEventType::BALANCE_UNLOCKED;

  return event;
}

CryptoNote::WalletEvent makeSyncProgressUpdatedEvent(uint32_t current, uint32_t total) {
  CryptoNote::WalletEvent event;
  event.type = CryptoNote::WalletEventType::SYNC_PROGRESS_UPDATED;
  event.synchronizationProgressUpdated.processedBlockCount = current;
  event.synchronizationProgressUpdated.totalBlockCount = total;

  return event;
}

CryptoNote::WalletEvent makeSyncCompletedEvent() {
  CryptoNote::WalletEvent event;
  event.type = CryptoNote::WalletEventType::SYNC_COMPLETED;

  return event;
}

size_t getTransactionSize(const ITransactionReader& transaction) {
  return transaction.getTransactionData().size();
}

}

namespace CryptoNote {

WalletGreen::WalletGreen(System::Dispatcher& dispatcher, const Currency& currency, INode& node, uint32_t transactionSoftLockTime) :
  m_dispatcher(dispatcher),
  m_currency(currency),
  m_node(node),
  m_blockchainSynchronizer(node, currency.genesisBlockHash()),
  m_synchronizer(currency, m_blockchainSynchronizer, node),
  m_eventOccured(m_dispatcher),
  m_readyEvent(m_dispatcher),
  m_transactionSoftLockTime(transactionSoftLockTime)
{
  m_upperTransactionSizeLimit = m_currency.maxTransactionSizeLimit();
  m_readyEvent.set();
}

WalletGreen::~WalletGreen() {
  if (m_state == WalletState::INITIALIZED) {
    doShutdown();
  }

  m_dispatcher.yield(); //let remote spawns finish
}

void WalletGreen::initialize(const std::string& password) {
  Crypto::PublicKey viewPublicKey;
  Crypto::SecretKey viewSecretKey;
  Crypto::generate_keys(viewPublicKey, viewSecretKey);

  initWithKeys(viewPublicKey, viewSecretKey, password);
}

void WalletGreen::initializeWithViewKey(const Crypto::SecretKey& viewSecretKey, const std::string& password) {
  Crypto::PublicKey viewPublicKey;
  if (!Crypto::secret_key_to_public_key(viewSecretKey, viewPublicKey)) {
    throw std::system_error(make_error_code(CryptoNote::error::KEY_GENERATION_ERROR));
  }

  initWithKeys(viewPublicKey, viewSecretKey, password);
}

void WalletGreen::shutdown() {
  throwIfNotInitialized();
  doShutdown();

  m_dispatcher.yield(); //let remote spawns finish
}

void WalletGreen::doShutdown() {
  m_blockchainSynchronizer.stop();
  m_blockchainSynchronizer.removeObserver(this);

  clearCaches();

  std::queue<WalletEvent> noEvents;
  std::swap(m_events, noEvents);

  m_state = WalletState::NOT_INITIALIZED;
}

void WalletGreen::clearCaches() {
  std::vector<AccountPublicAddress> subscriptions;
  m_synchronizer.getSubscriptions(subscriptions);
  std::for_each(subscriptions.begin(), subscriptions.end(), [this] (const AccountPublicAddress& address) { m_synchronizer.removeSubscription(address); });

  m_walletsContainer.clear();
  m_spentOutputs.clear();
  m_unlockTransactionsJob.clear();
  m_transactions.clear();
  m_transfers.clear();
  m_change.clear();
  m_actualBalance = 0;
  m_pendingBalance = 0;
  m_fusionTxsCache.clear();
}

void WalletGreen::initWithKeys(const Crypto::PublicKey& viewPublicKey, const Crypto::SecretKey& viewSecretKey, const std::string& password) {
  if (m_state != WalletState::NOT_INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::ALREADY_INITIALIZED));
  }

  throwIfStopped();

  m_viewPublicKey = viewPublicKey;
  m_viewSecretKey = viewSecretKey;
  m_password = password;

  m_blockchainSynchronizer.addObserver(this);

  m_state = WalletState::INITIALIZED;
}

void WalletGreen::save(std::ostream& destination, bool saveDetails, bool saveCache) {
  throwIfNotInitialized();
  throwIfStopped();

  if (m_walletsContainer.get<RandomAccessIndex>().size() != 0) {
    m_blockchainSynchronizer.stop();
  }

  unsafeSave(destination, saveDetails, saveCache);

  if (m_walletsContainer.get<RandomAccessIndex>().size() != 0) {
    m_blockchainSynchronizer.start();
  }
}

void WalletGreen::unsafeSave(std::ostream& destination, bool saveDetails, bool saveCache) {
  WalletSerializer s(
    *this,
    m_viewPublicKey,
    m_viewSecretKey,
    m_actualBalance,
    m_pendingBalance,
    m_walletsContainer,
    m_synchronizer,
    m_spentOutputs,
    m_unlockTransactionsJob,
    m_change,
    m_transactions,
    m_transfers,
    m_transactionSoftLockTime
  );

  StdOutputStream output(destination);
  s.save(m_password, output, saveDetails, saveCache);
}

void WalletGreen::load(std::istream& source, const std::string& password) {
  if (m_state != WalletState::NOT_INITIALIZED) {
    throw std::system_error(make_error_code(error::WRONG_STATE));
  }

  throwIfStopped();

  if (m_walletsContainer.get<RandomAccessIndex>().size() != 0) {
    m_blockchainSynchronizer.stop();
  }

  unsafeLoad(source, password);

  if (m_walletsContainer.get<RandomAccessIndex>().size() != 0) {
    m_blockchainSynchronizer.start();
  }

  m_state = WalletState::INITIALIZED;
}

void WalletGreen::unsafeLoad(std::istream& source, const std::string& password) {
  WalletSerializer s(
    *this,
    m_viewPublicKey,
    m_viewSecretKey,
    m_actualBalance,
    m_pendingBalance,
    m_walletsContainer,
    m_synchronizer,
    m_spentOutputs,
    m_unlockTransactionsJob,
    m_change,
    m_transactions,
    m_transfers,
    m_transactionSoftLockTime
  );

  StdInputStream inputStream(source);
  s.load(password, inputStream);

  m_password = password;
  m_blockchainSynchronizer.addObserver(this);
}

void WalletGreen::changePassword(const std::string& oldPassword, const std::string& newPassword) {
  throwIfNotInitialized();
  throwIfStopped();

  if (m_password.compare(oldPassword)) {
    throw std::system_error(make_error_code(error::WRONG_PASSWORD));
  }

  m_password = newPassword;
}

size_t WalletGreen::getAddressCount() const {
  throwIfNotInitialized();
  throwIfStopped();

  return m_walletsContainer.get<RandomAccessIndex>().size();
}

std::string WalletGreen::getAddress(size_t index) const {
  throwIfNotInitialized();
  throwIfStopped();

  if (index >= m_walletsContainer.get<RandomAccessIndex>().size()) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }

  const WalletRecord& wallet = m_walletsContainer.get<RandomAccessIndex>()[index];
  return m_currency.accountAddressAsString({ wallet.spendPublicKey, m_viewPublicKey });
}

KeyPair WalletGreen::getAddressSpendKey(size_t index) const {
  throwIfNotInitialized();
  throwIfStopped();

  if (index >= m_walletsContainer.get<RandomAccessIndex>().size()) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }

  const WalletRecord& wallet = m_walletsContainer.get<RandomAccessIndex>()[index];
  return {wallet.spendPublicKey, wallet.spendSecretKey};
}

KeyPair WalletGreen::getViewKey() const {
  throwIfNotInitialized();
  throwIfStopped();

  return {m_viewPublicKey, m_viewSecretKey};
}

std::string WalletGreen::createAddress() {
  KeyPair spendKey;
  Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);
  uint64_t creationTimestamp = static_cast<uint64_t>(time(nullptr));

  return doCreateAddress(spendKey.publicKey, spendKey.secretKey, creationTimestamp);
}

std::string WalletGreen::createAddress(const Crypto::SecretKey& spendSecretKey) {
  Crypto::PublicKey spendPublicKey;
  if (!Crypto::secret_key_to_public_key(spendSecretKey, spendPublicKey) ) {
    throw std::system_error(make_error_code(CryptoNote::error::KEY_GENERATION_ERROR));
  }

  return doCreateAddress(spendPublicKey, spendSecretKey, 0);
}

std::string WalletGreen::createAddress(const Crypto::PublicKey& spendPublicKey) {
  return doCreateAddress(spendPublicKey, NULL_SECRET_KEY, 0);
}

std::string WalletGreen::doCreateAddress(const Crypto::PublicKey& spendPublicKey, const Crypto::SecretKey& spendSecretKey, uint64_t creationTimestamp) {
  throwIfNotInitialized();
  throwIfStopped();

  if (m_walletsContainer.get<RandomAccessIndex>().size() != 0) {
    m_blockchainSynchronizer.stop();
  }

  try {
    addWallet(spendPublicKey, spendSecretKey, creationTimestamp);
  } catch (std::exception&) {
    if (m_walletsContainer.get<RandomAccessIndex>().size() != 0) {
      m_blockchainSynchronizer.start();
    }

    throw;
  }

  m_blockchainSynchronizer.start();

  return m_currency.accountAddressAsString({ spendPublicKey, m_viewPublicKey });
}

void WalletGreen::addWallet(const Crypto::PublicKey& spendPublicKey, const Crypto::SecretKey& spendSecretKey, uint64_t creationTimestamp) {
  auto& index = m_walletsContainer.get<KeysIndex>();

  auto trackingMode = getTrackingMode();

  if ((trackingMode == WalletTrackingMode::TRACKING && spendSecretKey != NULL_SECRET_KEY) ||
      (trackingMode == WalletTrackingMode::NOT_TRACKING && spendSecretKey == NULL_SECRET_KEY)) {
    throw std::system_error(make_error_code(error::BAD_ADDRESS));
  }

  auto insertIt = index.find(spendPublicKey);
  if (insertIt != index.end()) {
    throw std::system_error(make_error_code(error::ADDRESS_ALREADY_EXISTS));
  }

  AccountSubscription sub;
  sub.keys.address.viewPublicKey = m_viewPublicKey;
  sub.keys.address.spendPublicKey = spendPublicKey;
  sub.keys.viewSecretKey = m_viewSecretKey;
  sub.keys.spendSecretKey = spendSecretKey;
  sub.transactionSpendableAge = m_transactionSoftLockTime;
  sub.syncStart.height = 0;
  sub.syncStart.timestamp = std::max(creationTimestamp, ACCOUNT_CREATE_TIME_ACCURACY) - ACCOUNT_CREATE_TIME_ACCURACY;

  auto& trSubscription = m_synchronizer.addSubscription(sub);
  ITransfersContainer* container = &trSubscription.getContainer();

  WalletRecord wallet;
  wallet.spendPublicKey = spendPublicKey;
  wallet.spendSecretKey = spendSecretKey;
  wallet.container = container;
  wallet.creationTimestamp = static_cast<time_t>(creationTimestamp);
  trSubscription.addObserver(this);

  index.insert(insertIt, std::move(wallet));
}

void WalletGreen::deleteAddress(const std::string& address) {
  throwIfNotInitialized();
  throwIfStopped();

  CryptoNote::AccountPublicAddress pubAddr = parseAddress(address);

  auto it = m_walletsContainer.get<KeysIndex>().find(pubAddr.spendPublicKey);
  if (it == m_walletsContainer.get<KeysIndex>().end()) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }

  m_blockchainSynchronizer.stop();

  m_actualBalance -= it->actualBalance;
  m_pendingBalance -= it->pendingBalance;

  m_synchronizer.removeSubscription(pubAddr);

  m_spentOutputs.get<WalletIndex>().erase(&(*it));
  m_walletsContainer.get<KeysIndex>().erase(it);

  if (m_walletsContainer.get<RandomAccessIndex>().size() != 0) {
    m_blockchainSynchronizer.start();
  }
}

uint64_t WalletGreen::getActualBalance() const {
  throwIfNotInitialized();
  throwIfStopped();

  return m_actualBalance;
}

uint64_t WalletGreen::getActualBalance(const std::string& address) const {
  throwIfNotInitialized();
  throwIfStopped();

  const auto& wallet = getWalletRecord(address);
  return wallet.actualBalance;
}

uint64_t WalletGreen::getPendingBalance() const {
  throwIfNotInitialized();
  throwIfStopped();

  return m_pendingBalance;
}

uint64_t WalletGreen::getPendingBalance(const std::string& address) const {
  throwIfNotInitialized();
  throwIfStopped();

  const auto& wallet = getWalletRecord(address);
  return wallet.pendingBalance;
}

size_t WalletGreen::getTransactionCount() const {
  throwIfNotInitialized();
  throwIfStopped();

  return m_transactions.get<RandomAccessIndex>().size();
}

WalletTransaction WalletGreen::getTransaction(size_t transactionIndex) const {
  throwIfNotInitialized();
  throwIfStopped();

  if (m_transactions.size() <= transactionIndex) {
    throw std::system_error(make_error_code(CryptoNote::error::INDEX_OUT_OF_RANGE));
  }

  return m_transactions.get<RandomAccessIndex>()[transactionIndex];
}

size_t WalletGreen::getTransactionTransferCount(size_t transactionIndex) const {
  throwIfNotInitialized();
  throwIfStopped();

  auto bounds = getTransactionTransfers(transactionIndex);
  return static_cast<size_t>(std::distance(bounds.first, bounds.second));
}

WalletTransfer WalletGreen::getTransactionTransfer(size_t transactionIndex, size_t transferIndex) const {
  throwIfNotInitialized();
  throwIfStopped();

  auto bounds = getTransactionTransfers(transactionIndex);

  if (transferIndex >= static_cast<size_t>(std::distance(bounds.first, bounds.second))) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }

  auto it = bounds.first;
  std::advance(it, transferIndex);
  return it->second;
}

std::pair<WalletTransfers::const_iterator, WalletTransfers::const_iterator> WalletGreen::getTransactionTransfers(
  size_t transactionIndex) const {

  auto val = std::make_pair(transactionIndex, WalletTransfer());

  auto bounds = std::equal_range(m_transfers.begin(), m_transfers.end(), val, [] (const TransactionTransferPair& a, const TransactionTransferPair& b) {
    return a.first < b.first;
  });

  return bounds;
}

size_t WalletGreen::transfer(const WalletTransfer& destination,
  uint64_t fee,
  uint64_t mixIn,
  std::string const& extra,
  uint64_t unlockTimestamp)
{
  std::vector<WalletTransfer> destinations { destination };
  return transfer(destinations, fee, mixIn, extra, unlockTimestamp);
}

size_t WalletGreen::transfer(
  const std::vector<WalletTransfer>& destinations,
  uint64_t fee,
  uint64_t mixIn,
  const std::string& extra,
  uint64_t unlockTimestamp) {

  System::EventLock lk(m_readyEvent);

  throwIfNotInitialized();
  throwIfStopped();
  throwIfTrackingMode();

  return doTransfer(pickWalletsWithMoney(), destinations, fee, mixIn, extra, unlockTimestamp);
}

size_t WalletGreen::transfer(
    const std::string& sourceAddress,
    const WalletTransfer& destination,
    uint64_t fee,
    uint64_t mixIn,
    std::string const& extra,
    uint64_t unlockTimestamp) {
  std::vector<WalletTransfer> destinations { destination };
  return transfer(sourceAddress, destinations, fee, mixIn, extra, unlockTimestamp);
}

size_t WalletGreen::transfer(
    const std::string& sourceAddress,
    const std::vector<WalletTransfer>& destinations,
    uint64_t fee,
    uint64_t mixIn,
    const std::string& extra,
    uint64_t unlockTimestamp) {
  System::EventLock lk(m_readyEvent);

  throwIfNotInitialized();
  throwIfStopped();
  throwIfTrackingMode();

  WalletOuts wallet = pickWallet(sourceAddress);
  std::vector<WalletOuts> wallets;

  if (!wallet.outs.empty()) {
    wallets.push_back(wallet);
  }

  return doTransfer(std::move(wallets), destinations, fee, mixIn, extra, unlockTimestamp);
}

size_t WalletGreen::doTransfer(std::vector<WalletOuts>&& wallets,
    const std::vector<WalletTransfer>& destinations,
    uint64_t fee,
    uint64_t mixIn,
    const std::string& extra,
    uint64_t unlockTimestamp) {
  if (destinations.empty()) {
    throw std::system_error(make_error_code(error::ZERO_DESTINATION));
  }

  if (fee < m_currency.minimumFee()) {
    throw std::system_error(make_error_code(error::FEE_TOO_SMALL));
  }

  validateAddresses(destinations, m_currency);

  uint64_t neededMoney = countNeededMoney(destinations, fee);

  std::vector<OutputToTransfer> selectedTransfers;
  uint64_t foundMoney = selectTransfers(neededMoney, mixIn == 0, m_currency.defaultDustThreshold(), std::move(wallets), selectedTransfers);

  if (foundMoney < neededMoney) {
    throw std::system_error(make_error_code(error::WRONG_AMOUNT), "Not enough money");
  }

  typedef CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount outs_for_amount;
  std::vector<outs_for_amount> mixinResult;

  if (mixIn != 0) {
    requestMixinOuts(selectedTransfers, mixIn, mixinResult);
  }

  std::vector<InputInfo> keysInfo;
  prepareInputs(selectedTransfers, mixinResult, mixIn, keysInfo);

  WalletTransfer changeDestination;
  changeDestination.address = m_currency.accountAddressAsString({ m_walletsContainer.get<RandomAccessIndex>()[0].spendPublicKey, m_viewPublicKey });
  changeDestination.amount = foundMoney - neededMoney;

  std::vector<ReceiverAmounts> decomposedOutputs;
  splitDestinations(destinations, changeDestination, m_currency.defaultDustThreshold(), m_currency, decomposedOutputs);

  std::unique_ptr<ITransaction> tx = makeTransaction(decomposedOutputs, keysInfo, extra, unlockTimestamp);

  size_t txId = insertOutgoingTransaction(tx->getTransactionHash(), -static_cast<int64_t>(neededMoney), fee, tx->getExtra(), unlockTimestamp);
  pushBackOutgoingTransfers(txId, destinations);
  m_fusionTxsCache.emplace(txId, false);

  markOutputsSpent(tx->getTransactionHash(), selectedTransfers);

  try {
    sendTransaction(tx.get());
  } catch (std::exception&) {
    deleteSpentOutputs(tx->getTransactionHash());
    pushEvent(makeTransactionCreatedEvent(txId));
    throw;
  }

  auto txIt = m_transactions.get<RandomAccessIndex>().begin();
  std::advance(txIt, txId);
  m_transactions.get<RandomAccessIndex>().modify(txIt,
    [] (WalletTransaction& tx) { tx.state = WalletTransactionState::SUCCEEDED; });

  m_change[tx->getTransactionHash()] = changeDestination.amount;
  updateUsedWalletsBalances(selectedTransfers);

  pushEvent(makeTransactionCreatedEvent(txId));

  return txId;
}

void WalletGreen::pushBackOutgoingTransfers(size_t txId, const std::vector<WalletTransfer> &destinations) {
  for (const auto& dest: destinations) {
    WalletTransfer d { dest.address, -dest.amount };
    m_transfers.push_back(std::make_pair(txId, d));
  }
}

size_t WalletGreen::insertOutgoingTransaction(const Hash& transactionHash, int64_t totalAmount, uint64_t fee, const BinaryArray& extra, uint64_t unlockTimestamp) {
  WalletTransaction insertTx;
  insertTx.state = WalletTransactionState::FAILED;
  insertTx.creationTime = static_cast<uint64_t>(time(nullptr));
  insertTx.unlockTime = unlockTimestamp;
  insertTx.blockHeight = CryptoNote::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
  insertTx.extra.assign(reinterpret_cast<const char*>(extra.data()), extra.size());
  insertTx.fee = fee;
  insertTx.hash = transactionHash;
  insertTx.totalAmount = totalAmount;
  insertTx.timestamp = 0; //0 until included in a block
  insertTx.isBase = false;

  size_t txId = m_transactions.get<RandomAccessIndex>().size();
  m_transactions.get<RandomAccessIndex>().push_back(std::move(insertTx));

  return txId;
}

bool WalletGreen::transactionExists(const Hash& hash) {
  auto& hashIndex = m_transactions.get<TransactionIndex>();
  auto it = hashIndex.find(hash);
  return it != hashIndex.end();
}

bool WalletGreen::updateWalletTransactionInfo(const Hash& hash, const CryptoNote::TransactionInformation& info) {
  auto& hashIndex = m_transactions.get<TransactionIndex>();

  bool updated = false;
  auto it = hashIndex.find(hash);
  if (it != hashIndex.end()) {
    bool r = hashIndex.modify(it, [&info, &updated] (WalletTransaction& transaction) {
        if (transaction.blockHeight != info.blockHeight) {
          transaction.blockHeight = info.blockHeight;
          updated = true;
        }

        if (transaction.timestamp != info.timestamp) {
          transaction.timestamp = info.timestamp;
          updated = true;
        }

        if (transaction.state != WalletTransactionState::SUCCEEDED) {
          //transaction may be deleted first then added again
          transaction.state = WalletTransactionState::SUCCEEDED;
          updated = true;
        }

        // Fix LegacyWallet error. Some old versions didn't fill extra field
        if (transaction.extra.empty() && !info.extra.empty()) {
          transaction.extra = Common::asString(info.extra);
          updated = true;
        }

        bool isBase = info.totalAmountIn == 0;
        if (transaction.isBase != isBase) {
          transaction.isBase = isBase;
          updated = true;
        }
    });

    assert(r);
    return updated;
  }

  throw std::system_error(make_error_code(std::errc::invalid_argument));
}

size_t WalletGreen::insertBlockchainTransaction(const TransactionInformation& info, int64_t txBalance) {
  auto& index = m_transactions.get<RandomAccessIndex>();

  WalletTransaction tx;
  tx.state = WalletTransactionState::SUCCEEDED;
  tx.timestamp = info.timestamp;
  tx.blockHeight = info.blockHeight;
  tx.hash = info.transactionHash;
  tx.fee = info.totalAmountIn - info.totalAmountOut;
  tx.unlockTime = info.unlockTime;
  tx.extra.assign(reinterpret_cast<const char*>(info.extra.data()), info.extra.size());
  tx.totalAmount = txBalance;
  tx.creationTime = info.timestamp;
  tx.isBase = info.totalAmountIn == 0;

  index.push_back(std::move(tx));
  return index.size() - 1;
}

void WalletGreen::insertIncomingTransfer(size_t txId, const std::string& address, int64_t amount) {
  auto it = std::upper_bound(m_transfers.begin(), m_transfers.end(), txId, [] (size_t val, const TransactionTransferPair& a) {
    return val < a.first;
  });

  WalletTransfer tr { address, amount };
  m_transfers.insert(it, std::make_pair(txId, std::move(tr)));
}

std::unique_ptr<CryptoNote::ITransaction> WalletGreen::makeTransaction(const std::vector<ReceiverAmounts>& decomposedOutputs,
  std::vector<InputInfo>& keysInfo, const std::string& extra, uint64_t unlockTimestamp) {

  std::unique_ptr<ITransaction> tx = createTransaction();

  for (const auto& output: decomposedOutputs) {
    for (auto amount: output.amounts) {
      tx->addOutput(amount, output.receiver);
    }
  }

  tx->setUnlockTime(unlockTimestamp);
  tx->appendExtra(Common::asBinaryArray(extra));

  for (auto& input: keysInfo) {
    tx->addInput(makeAccountKeys(*input.walletRecord), input.keyInfo, input.ephKeys);
  }

  size_t i = 0;
  for(auto& input: keysInfo) {
    tx->signInputKey(i++, input.keyInfo, input.ephKeys);
  }

  return tx;
}

void WalletGreen::sendTransaction(ITransaction* tx) {
  System::Event completion(m_dispatcher);
  std::error_code ec;
  CryptoNote::Transaction oldTxFormat;

  const auto& ba = tx->getTransactionData();

  if (ba.size() > m_upperTransactionSizeLimit) {
    throw std::system_error(make_error_code(error::TRANSACTION_SIZE_TOO_BIG));
  }

  if (!fromBinaryArray(oldTxFormat, ba)) {
    throw std::system_error(make_error_code(error::INTERNAL_WALLET_ERROR));
  }

  throwIfStopped();
  m_node.relayTransaction(oldTxFormat, [&ec, &completion, this] (std::error_code error) {
    ec = error;
    this->m_dispatcher.remoteSpawn(std::bind(asyncRequestCompletion, std::ref(completion)));
  });
  completion.wait();

  if (ec) {
    throw std::system_error(ec);
  }
}

AccountKeys WalletGreen::makeAccountKeys(const WalletRecord& wallet) const {
  AccountKeys keys;
  keys.address.spendPublicKey = wallet.spendPublicKey;
  keys.address.viewPublicKey = m_viewPublicKey;
  keys.spendSecretKey = wallet.spendSecretKey;
  keys.viewSecretKey = m_viewSecretKey;

  return keys;
}

void WalletGreen::requestMixinOuts(
  const std::vector<OutputToTransfer>& selectedTransfers,
  uint64_t mixIn,
  std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult) {

  std::vector<uint64_t> amounts;
  for (const auto& out: selectedTransfers) {
    amounts.push_back(out.out.amount);
  }

  System::Event requestFinished(m_dispatcher);
  std::error_code mixinError;

  throwIfStopped();

  m_node.getRandomOutsByAmounts(std::move(amounts), mixIn, mixinResult, [&requestFinished, &mixinError, this] (std::error_code ec) {
    mixinError = ec;
    this->m_dispatcher.remoteSpawn(std::bind(asyncRequestCompletion, std::ref(requestFinished)));
  });

  requestFinished.wait();

  checkIfEnoughMixins(mixinResult, mixIn);

  if (mixinError) {
    throw std::system_error(mixinError);
  }
}

uint64_t WalletGreen::selectTransfers(
  uint64_t neededMoney,
  bool dust,
  uint64_t dustThreshold,
  std::vector<WalletOuts>&& wallets,
  std::vector<OutputToTransfer>& selectedTransfers) {

  uint64_t foundMoney = 0;

  std::vector<WalletOuts> walletOuts = wallets;
  std::default_random_engine randomGenerator(Crypto::rand<std::default_random_engine::result_type>());

  while (foundMoney < neededMoney && !walletOuts.empty()) {
    std::uniform_int_distribution<size_t> walletsDistribution(0, walletOuts.size() - 1);

    size_t walletIndex = walletsDistribution(randomGenerator);
    std::vector<TransactionOutputInformation>& addressOuts = walletOuts[walletIndex].outs;

    assert(addressOuts.size() > 0);
    std::uniform_int_distribution<size_t> outDistribution(0, addressOuts.size() - 1);
    size_t outIndex = outDistribution(randomGenerator);

    TransactionOutputInformation out = addressOuts[outIndex];
    if (!isOutputUsed(out) && (out.amount > dustThreshold || dust)) {
      if (out.amount <= dustThreshold) {
        dust = false;
      }

      foundMoney += out.amount;

      selectedTransfers.push_back( { std::move(out), walletOuts[walletIndex].wallet } );
    }

    addressOuts.erase(addressOuts.begin() + outIndex);
    if (addressOuts.empty()) {
      walletOuts.erase(walletOuts.begin() + walletIndex);
    }
  }

  if (!dust) {
    return foundMoney;
  }

  for (const auto& addressOuts : walletOuts) {
    auto it = std::find_if(addressOuts.outs.begin(), addressOuts.outs.end(),
      [dustThreshold, this] (const TransactionOutputInformation& out) {
        return out.amount <= dustThreshold && (!this->isOutputUsed(out));
      }
    );

    if (it != addressOuts.outs.end()) {
      foundMoney += it->amount;
      selectedTransfers.push_back({ *it, addressOuts.wallet });
      break;
    }
  }

  return foundMoney;
};

std::vector<WalletGreen::WalletOuts> WalletGreen::pickWalletsWithMoney() const {
  auto& walletsIndex = m_walletsContainer.get<RandomAccessIndex>();

  std::vector<WalletOuts> walletOuts;
  for (const auto& wallet: walletsIndex) {
    if (wallet.actualBalance == 0) {
      continue;
    }

    ITransfersContainer* container = wallet.container;

    WalletOuts outs;
    container->getOutputs(outs.outs, ITransfersContainer::IncludeKeyUnlocked);
    outs.wallet = const_cast<WalletRecord *>(&wallet);

    walletOuts.push_back(std::move(outs));
  };

  return walletOuts;
}

WalletGreen::WalletOuts WalletGreen::pickWallet(const std::string& address) {
  const auto& wallet = getWalletRecord(address);

  ITransfersContainer* container = wallet.container;
  WalletOuts outs;
  container->getOutputs(outs.outs, ITransfersContainer::IncludeKeyUnlocked);
  outs.wallet = const_cast<WalletRecord *>(&wallet);

  return outs;
}

void WalletGreen::splitDestinations(const std::vector<CryptoNote::WalletTransfer>& destinations,
  const CryptoNote::WalletTransfer& changeDestination,
  uint64_t dustThreshold,
  const CryptoNote::Currency& currency,
  std::vector<ReceiverAmounts>& decomposedOutputs) {

  for (const auto& destination: destinations) {
    ReceiverAmounts receiverAmounts;

    parseAddressString(destination.address, currency, receiverAmounts.receiver);
    decomposeAmount(destination.amount, dustThreshold, receiverAmounts.amounts);

    decomposedOutputs.push_back(std::move(receiverAmounts));
  }

  ReceiverAmounts changeAmounts;
  parseAddressString(changeDestination.address, currency, changeAmounts.receiver);
  decomposeAmount(changeDestination.amount, dustThreshold, changeAmounts.amounts);

  decomposedOutputs.push_back(std::move(changeAmounts));
}

void WalletGreen::prepareInputs(
  const std::vector<OutputToTransfer>& selectedTransfers,
  std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult,
  uint64_t mixIn,
  std::vector<InputInfo>& keysInfo) {

  typedef CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry out_entry;

  size_t i = 0;
  for (const auto& input: selectedTransfers) {
    TransactionTypes::InputKeyInfo keyInfo;
    keyInfo.amount = input.out.amount;

    if(mixinResult.size()) {
      std::sort(mixinResult[i].outs.begin(), mixinResult[i].outs.end(),
        [] (const out_entry& a, const out_entry& b) { return a.global_amount_index < b.global_amount_index; });
      for (auto& fakeOut: mixinResult[i].outs) {

        if (input.out.globalOutputIndex == fakeOut.global_amount_index) {
          continue;
        }

        TransactionTypes::GlobalOutput globalOutput;
        globalOutput.outputIndex = static_cast<uint32_t>(fakeOut.global_amount_index);
        globalOutput.targetKey = reinterpret_cast<PublicKey&>(fakeOut.out_key);
        keyInfo.outputs.push_back(std::move(globalOutput));
        if(keyInfo.outputs.size() >= mixIn)
          break;
      }
    }

    //paste real transaction to the random index
    auto insertIn = std::find_if(keyInfo.outputs.begin(), keyInfo.outputs.end(), [&](const TransactionTypes::GlobalOutput& a) {
      return a.outputIndex >= input.out.globalOutputIndex;
    });

    TransactionTypes::GlobalOutput realOutput;
    realOutput.outputIndex = input.out.globalOutputIndex;
    realOutput.targetKey = reinterpret_cast<const PublicKey&>(input.out.outputKey);

    auto insertedIn = keyInfo.outputs.insert(insertIn, realOutput);

    keyInfo.realOutput.transactionPublicKey = reinterpret_cast<const PublicKey&>(input.out.transactionPublicKey);
    keyInfo.realOutput.transactionIndex = static_cast<size_t>(insertedIn - keyInfo.outputs.begin());
    keyInfo.realOutput.outputInTransaction = input.out.outputInTransaction;

    //Important! outputs in selectedTransfers and in keysInfo must have the same order!
    InputInfo inputInfo;
    inputInfo.keyInfo = std::move(keyInfo);
    inputInfo.walletRecord = input.wallet;
    keysInfo.push_back(std::move(inputInfo));
    ++i;
  }
}

void WalletGreen::start() {
  m_stopped = false;
}

void WalletGreen::stop() {
  m_stopped = true;
  m_eventOccured.set();
}

WalletEvent WalletGreen::getEvent() {
  throwIfNotInitialized();
  throwIfStopped();

  while(m_events.empty()) {
    m_eventOccured.wait();
    m_eventOccured.clear();
    throwIfStopped();
  }

  WalletEvent event = std::move(m_events.front());
  m_events.pop();

  return event;
}

void WalletGreen::throwIfNotInitialized() const {
  if (m_state != WalletState::INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }
}

void WalletGreen::onError(ITransfersSubscription* object, uint32_t height, std::error_code ec) {
}

void WalletGreen::synchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount) {
  m_dispatcher.remoteSpawn( [processedBlockCount, totalBlockCount, this] () { onSynchronizationProgressUpdated(processedBlockCount, totalBlockCount); } );
}

void WalletGreen::synchronizationCompleted(std::error_code result) {
  m_dispatcher.remoteSpawn([this] () { onSynchronizationCompleted(); } );
}

void WalletGreen::onSynchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount) {
  assert(processedBlockCount > 0);

  System::EventLock lk(m_readyEvent);

  if (m_state == WalletState::NOT_INITIALIZED) {
    return;
  }

  pushEvent(makeSyncProgressUpdatedEvent(processedBlockCount, totalBlockCount));

  uint32_t currentHeight = processedBlockCount - 1;
  unlockBalances(currentHeight);
}

void WalletGreen::onSynchronizationCompleted() {
  System::EventLock lk(m_readyEvent);

  if (m_state == WalletState::NOT_INITIALIZED) {
    return;
  }

  pushEvent(makeSyncCompletedEvent());
}

void WalletGreen::unlockBalances(uint32_t height) {
  auto& index = m_unlockTransactionsJob.get<BlockHeightIndex>();
  auto upper = index.upper_bound(height);

  if (index.begin() != upper) {
    for (auto it = index.begin(); it != upper; ++it) {
      updateBalance(it->container);
    }

    index.erase(index.begin(), upper);
    pushEvent(makeMoneyUnlockedEvent());
  }
}

void WalletGreen::onTransactionUpdated(ITransfersSubscription* object, const Hash& transactionHash) {
  m_dispatcher.remoteSpawn([object, transactionHash, this] () { this->transactionUpdated(object, transactionHash); } );
}

void WalletGreen::transactionUpdated(ITransfersSubscription* object, const Hash& transactionHash) {
  System::EventLock lk(m_readyEvent);

  if (m_state == WalletState::NOT_INITIALIZED) {
    return;
  }

  CryptoNote::ITransfersContainer* container = &object->getContainer();

  deleteSpentOutputs(transactionHash);

  CryptoNote::TransactionInformation info;
  int64_t txBalance;
  bool found = container->getTransactionInformation(transactionHash, info, txBalance);
  assert(found);

  if (transactionExists(info.transactionHash)) {
    bool updated = updateWalletTransactionInfo(info.transactionHash, info);

    auto id = getTransactionId(info.transactionHash);

    if (updated) {
      pushEvent(makeTransactionUpdatedEvent(id));
    }
  } else {
    auto id = insertBlockchainTransaction(info, txBalance);
    if (txBalance > 0) {
      AccountPublicAddress walletAddress{ getWalletRecord(container).spendPublicKey, m_viewPublicKey };
      insertIncomingTransfer(id, m_currency.accountAddressAsString(walletAddress), txBalance);
    }

    m_fusionTxsCache.emplace(id, isFusionTransaction(m_transactions.get<RandomAccessIndex>()[id]));

    pushEvent(makeTransactionCreatedEvent(id));
  }

  m_change.erase(transactionHash);

  if (info.blockHeight != CryptoNote::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) {
    uint32_t unlockHeight = std::max(info.blockHeight + m_transactionSoftLockTime, static_cast<uint32_t>(info.unlockTime));
    insertUnlockTransactionJob(transactionHash, unlockHeight, container);
  }

  updateBalance(container);
}

void WalletGreen::pushEvent(const WalletEvent& event) {
  m_events.push(event);
  m_eventOccured.set();
}

size_t WalletGreen::getTransactionId(const Hash& transactionHash) const {
  auto it = m_transactions.get<TransactionIndex>().find(transactionHash);

  if (it == m_transactions.get<TransactionIndex>().end()) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }

  auto rndIt = m_transactions.project<RandomAccessIndex>(it);
  auto txId = std::distance(m_transactions.get<RandomAccessIndex>().begin(), rndIt);

  return txId;
}

void WalletGreen::onTransactionDeleted(ITransfersSubscription* object, const Hash& transactionHash) {
  m_dispatcher.remoteSpawn([object, transactionHash, this] () {
    this->transactionDeleted(object, transactionHash); });
}

void WalletGreen::transactionDeleted(ITransfersSubscription* object, const Hash& transactionHash) {
  System::EventLock lk(m_readyEvent);

  if (m_state == WalletState::NOT_INITIALIZED) {
    return;
  }

  auto it = m_transactions.get<TransactionIndex>().find(transactionHash);
  if (it == m_transactions.get<TransactionIndex>().end()) {
    return;
  }

  CryptoNote::ITransfersContainer* container = &object->getContainer();
  deleteUnlockTransactionJob(transactionHash);
  m_change.erase(transactionHash);
  deleteSpentOutputs(transactionHash);

  m_transactions.get<TransactionIndex>().modify(it, [] (CryptoNote::WalletTransaction& tx) {
    tx.state = WalletTransactionState::CANCELLED;
    tx.blockHeight = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
  });

  auto rndIt = m_transactions.project<RandomAccessIndex>(it);
  auto id = std::distance(m_transactions.get<RandomAccessIndex>().begin(), rndIt);

  updateBalance(container);
  pushEvent(makeTransactionUpdatedEvent(id));
}

void WalletGreen::insertUnlockTransactionJob(const Hash& transactionHash, uint32_t blockHeight, CryptoNote::ITransfersContainer* container) {
  auto& index = m_unlockTransactionsJob.get<BlockHeightIndex>();
  index.insert( { blockHeight, container, transactionHash } );
}

void WalletGreen::deleteUnlockTransactionJob(const Hash& transactionHash) {
  auto& index = m_unlockTransactionsJob.get<TransactionHashIndex>();
  index.erase(transactionHash);
}

void WalletGreen::updateBalance(CryptoNote::ITransfersContainer* container) {
  auto it = m_walletsContainer.get<TransfersContainerIndex>().find(container);

  if (it == m_walletsContainer.get<TransfersContainerIndex>().end()) {
    return;
  }

  uint64_t actual = container->balance(ITransfersContainer::IncludeAllUnlocked);
  uint64_t pending = container->balance(ITransfersContainer::IncludeAllLocked);

  uint64_t unconfirmedBalance = countSpentBalance(&(*it));

  actual -= unconfirmedBalance;

  //xxx: i don't like this special case. Decompose this function
  if (container == m_walletsContainer.get<RandomAccessIndex>()[0].container) {
    uint64_t change = 0;
    std::for_each(m_change.begin(), m_change.end(), [&change] (const TransactionChanges::value_type& item) { change += item.second; });
    pending += change;
  }

  if (it->actualBalance < actual) {
    m_actualBalance += actual - it->actualBalance;
  } else {
    m_actualBalance -= it->actualBalance - actual;
  }

  if (it->pendingBalance < pending) {
    m_pendingBalance += pending - it->pendingBalance;
  } else {
    m_pendingBalance -= it->pendingBalance - pending;
  }

  m_walletsContainer.get<TransfersContainerIndex>().modify(it, [actual, pending] (WalletRecord& wallet) {
    wallet.actualBalance = actual;
    wallet.pendingBalance = pending;
  });
}

const WalletRecord& WalletGreen::getWalletRecord(const PublicKey& key) const {
  auto it = m_walletsContainer.get<KeysIndex>().find(key);
  if (it == m_walletsContainer.get<KeysIndex>().end()) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }

  return *it;
}

const WalletRecord& WalletGreen::getWalletRecord(const std::string& address) const {
  CryptoNote::AccountPublicAddress pubAddr = parseAddress(address);
  return getWalletRecord(pubAddr.spendPublicKey);
}

const WalletRecord& WalletGreen::getWalletRecord(CryptoNote::ITransfersContainer* container) const {
  auto it = m_walletsContainer.get<TransfersContainerIndex>().find(container);
  if (it == m_walletsContainer.get<TransfersContainerIndex>().end()) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }

  return *it;
}

CryptoNote::AccountPublicAddress WalletGreen::parseAddress(const std::string& address) const {
  CryptoNote::AccountPublicAddress pubAddr;

  if (!m_currency.parseAccountAddressString(address, pubAddr)) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }

  return pubAddr;
}

bool WalletGreen::isOutputUsed(const TransactionOutputInformation& out) const {
  return m_spentOutputs.get<TransactionOutputIndex>().find(boost::make_tuple(out.transactionHash, out.outputInTransaction))
    !=
    m_spentOutputs.get<TransactionOutputIndex>().end();
}

void WalletGreen::markOutputsSpent(const Hash& transactionHash,const std::vector<OutputToTransfer>& selectedTransfers) {
  auto& index = m_spentOutputs.get<TransactionOutputIndex>();

  for (const auto& output: selectedTransfers) {
    index.insert( {output.out.amount, output.out.transactionHash, output.out.outputInTransaction, output.wallet, transactionHash} );
  }
}

void WalletGreen::deleteSpentOutputs(const Hash& transactionHash) {
  auto& index = m_spentOutputs.get<TransactionIndex>();
  index.erase(transactionHash);
}

uint64_t WalletGreen::countSpentBalance(const WalletRecord* wallet) {
  uint64_t amount = 0;

  auto bounds = m_spentOutputs.get<WalletIndex>().equal_range(wallet);
  for (auto it = bounds.first; it != bounds.second; ++it) {
    amount += it->amount;
  }

  return amount;
}

void WalletGreen::updateUsedWalletsBalances(const std::vector<OutputToTransfer>& selectedTransfers) {
  std::set<WalletRecord *> wallets;

  // wallet #0 recieves change, so we have to update it after transfer
  wallets.insert(const_cast<WalletRecord* >(&m_walletsContainer.get<RandomAccessIndex>()[0]));

  std::for_each(selectedTransfers.begin(), selectedTransfers.end(), [&wallets] (const OutputToTransfer& output) { wallets.insert(output.wallet); } );
  std::for_each(wallets.begin(), wallets.end(), [this] (WalletRecord* wallet) {
    this->updateBalance(wallet->container);
  });
}

void WalletGreen::throwIfStopped() const {
  if (m_stopped) {
    throw std::system_error(make_error_code(error::OPERATION_CANCELLED));
  }
}

void WalletGreen::throwIfTrackingMode() const {
  if (getTrackingMode() == WalletTrackingMode::TRACKING) {
    throw std::system_error(make_error_code(error::TRACKING_MODE));
  }
}

WalletGreen::WalletTrackingMode WalletGreen::getTrackingMode() const {
  if (m_walletsContainer.get<RandomAccessIndex>().empty()) {
    return WalletTrackingMode::NO_ADDRESSES;
  }

  return m_walletsContainer.get<RandomAccessIndex>().begin()->spendSecretKey == NULL_SECRET_KEY ?
        WalletTrackingMode::TRACKING : WalletTrackingMode::NOT_TRACKING;
}

size_t WalletGreen::createFusionTransaction(uint64_t threshold, uint64_t mixin) {
  System::EventLock lk(m_readyEvent);

  throwIfNotInitialized();
  throwIfStopped();

  //TODO: check if wallet is not in tracking mode
  const size_t MAX_FUSION_OUTPUT_COUNT = 4;

  if (threshold <= m_currency.defaultDustThreshold()) {
    throw std::runtime_error("Threshold must be greater than " + std::to_string(m_currency.defaultDustThreshold()));
  }

  if (m_walletsContainer.get<RandomAccessIndex>().size() == 0) {
    throw std::runtime_error("You must have at least one address");
  }

  size_t estimatedFusionInputsCount = m_currency.getApproximateMaximumInputCount(m_currency.fusionTxMaxSize(), MAX_FUSION_OUTPUT_COUNT, mixin);
  if (estimatedFusionInputsCount < m_currency.fusionTxMinInputCount()) {
    throw std::system_error(make_error_code(error::MIXIN_COUNT_TOO_BIG));
  }

  std::vector<OutputToTransfer> fusionInputs = pickRandomFusionInputs(threshold, m_currency.fusionTxMinInputCount(), estimatedFusionInputsCount);
  if (fusionInputs.size() < m_currency.fusionTxMinInputCount()) {
    //nothing to optimize
    return WALLET_INVALID_TRANSACTION_ID;
  }

  typedef CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount outs_for_amount;
  std::vector<outs_for_amount> mixinResult;
  if (mixin != 0) {
    requestMixinOuts(fusionInputs, mixin, mixinResult);
  }

  std::vector<InputInfo> keysInfo;
  prepareInputs(fusionInputs, mixinResult, mixin, keysInfo);

  std::unique_ptr<ITransaction> fusionTransaction;
  size_t transactionSize;
  int round = 0;
  uint64_t transactionAmount;
  do {
    if (round != 0) {
      fusionInputs.pop_back();
      keysInfo.pop_back();
    }

    uint64_t inputsAmount = std::accumulate(fusionInputs.begin(), fusionInputs.end(), static_cast<uint64_t>(0), [] (uint64_t amount, const OutputToTransfer& input) {
      return amount + input.out.amount;
    });

    transactionAmount = inputsAmount;

    ReceiverAmounts decomposedOutputs = decomposeFusionOutputs(inputsAmount);
    assert(decomposedOutputs.amounts.size() <= MAX_FUSION_OUTPUT_COUNT);

    fusionTransaction = makeTransaction(std::vector<ReceiverAmounts>{decomposedOutputs}, keysInfo, "", 0);

    transactionSize = getTransactionSize(*fusionTransaction);
    ++round;
  } while (transactionSize > m_currency.fusionTxMaxSize() && fusionInputs.size() >= m_currency.fusionTxMinInputCount());

  if (fusionInputs.size() < m_currency.fusionTxMinInputCount()) {
    throw std::runtime_error("Unable to create fusion transaction");
  }

  size_t transactionId = insertOutgoingTransaction(fusionTransaction->getTransactionHash(), 0, 0, fusionTransaction->getExtra(), 0);
  m_fusionTxsCache.emplace(transactionId, true);

  WalletTransfer destination = {m_currency.accountAddressAsString({m_walletsContainer.get<RandomAccessIndex>().begin()->spendPublicKey,
                                                                   m_viewPublicKey }), 0};
  pushBackOutgoingTransfers(transactionId, std::vector<WalletTransfer> {destination});

  markOutputsSpent(fusionTransaction->getTransactionHash(), fusionInputs);

  try {
    sendTransaction(fusionTransaction.get());
  } catch (std::exception&) {
    deleteSpentOutputs(fusionTransaction->getTransactionHash());
    pushEvent(makeTransactionCreatedEvent(transactionId));
    throw;
  }

  auto txIt = m_transactions.get<RandomAccessIndex>().begin();
  std::advance(txIt, transactionId);
  m_transactions.get<RandomAccessIndex>().modify(txIt,
    [] (WalletTransaction& tx) { tx.state = WalletTransactionState::SUCCEEDED; });

  m_change[fusionTransaction->getTransactionHash()] = transactionAmount;

  updateUsedWalletsBalances(fusionInputs);

  pushEvent(makeTransactionCreatedEvent(transactionId));

  return transactionId;
}

WalletGreen::ReceiverAmounts WalletGreen::decomposeFusionOutputs(uint64_t inputsAmount) {
  assert(m_walletsContainer.get<RandomAccessIndex>().size() > 0);

  WalletGreen::ReceiverAmounts outputs;
  outputs.receiver = {m_walletsContainer.get<RandomAccessIndex>().begin()->spendPublicKey, m_viewPublicKey};

  decomposeAmount(inputsAmount, 0, outputs.amounts);
  std::sort(outputs.amounts.begin(), outputs.amounts.end());

  return outputs;
}

bool WalletGreen::isFusionTransaction(size_t transactionId) const {
  throwIfNotInitialized();
  throwIfStopped();

  if (m_transactions.size() <= transactionId) {
    throw std::system_error(make_error_code(CryptoNote::error::INDEX_OUT_OF_RANGE));
  }

  auto isFusionIter = m_fusionTxsCache.find(transactionId);
  if (isFusionIter != m_fusionTxsCache.end()) {
    return isFusionIter->second;
  }

  bool result = isFusionTransaction(m_transactions.get<RandomAccessIndex>()[transactionId]);
  m_fusionTxsCache.emplace(transactionId, result);
  return result;
}

bool WalletGreen::isFusionTransaction(const WalletTransaction& walletTx) const {
  if (walletTx.fee != 0) {
    return false;
  }

  uint64_t inputsSum = 0;
  uint64_t outputsSum = 0;
  std::vector<uint64_t> outputsAmounts;
  std::vector<uint64_t> inputsAmounts;
  TransactionInformation txInfo;
  bool gotTx = false;
  const auto& walletsIndex = m_walletsContainer.get<RandomAccessIndex>();
  for (const WalletRecord& wallet : walletsIndex) {
    for (const TransactionOutputInformation& output : wallet.container->getTransactionOutputs(walletTx.hash, ITransfersContainer::IncludeTypeKey | ITransfersContainer::IncludeStateAll)) {
      if (outputsAmounts.size() <= output.outputInTransaction) {
        outputsAmounts.resize(output.outputInTransaction + 1, 0);
      }

      assert(output.amount != 0);
      assert(outputsAmounts[output.outputInTransaction] == 0);
      outputsAmounts[output.outputInTransaction] = output.amount;
      outputsSum += output.amount;
    }

    for (const TransactionOutputInformation& input : wallet.container->getTransactionInputs(walletTx.hash, ITransfersContainer::IncludeTypeKey)) {
      inputsSum += input.amount;
      inputsAmounts.push_back(input.amount);
    }

    if (!gotTx) {
      int64_t ignore;
      gotTx = wallet.container->getTransactionInformation(walletTx.hash, txInfo, ignore);
    }
  }

  if (!gotTx) {
    return false;
  }

  if (outputsSum != inputsSum || outputsSum != txInfo.totalAmountOut || inputsSum != txInfo.totalAmountIn) {
    return false;
  } else {
    return m_currency.isFusionTransaction(inputsAmounts, outputsAmounts, 0); //size = 0 here because can't get real size of tx in wallet.
  }
}

IFusionManager::EstimateResult WalletGreen::estimate(uint64_t threshold) const {
  throwIfNotInitialized();
  throwIfStopped();

  IFusionManager::EstimateResult result{0, 0};
  auto walletOuts = pickWalletsWithMoney();
  std::array<size_t, std::numeric_limits<uint64_t>::digits10 + 1> bucketSizes;
  bucketSizes.fill(0);
  for (size_t walletIndex = 0; walletIndex < walletOuts.size(); ++walletIndex) {
    for (auto& out : walletOuts[walletIndex].outs) {
      uint8_t powerOfTen = 0;
      if (m_currency.isAmountApplicableInFusionTransactionInput(out.amount, threshold, powerOfTen) && !isOutputUsed(out)) {
        assert(powerOfTen < std::numeric_limits<uint64_t>::digits10 + 1);
        bucketSizes[powerOfTen]++;
      }
    }

    result.totalOutputCount += walletOuts[walletIndex].outs.size();
  }

  for (auto bucketSize : bucketSizes) {
    if (bucketSize >= m_currency.fusionTxMinInputCount()) {
      result.fusionReadyCount += bucketSize;
    }
  }

  return result;
}

std::vector<WalletGreen::OutputToTransfer> WalletGreen::pickRandomFusionInputs(uint64_t threshold, size_t minInputCount, size_t maxInputCount) {
  std::vector<WalletGreen::OutputToTransfer> allFusionReadyOuts;
  auto walletOuts = pickWalletsWithMoney();
  std::array<size_t, std::numeric_limits<uint64_t>::digits10 + 1> bucketSizes;
  bucketSizes.fill(0);
  for (size_t walletIndex = 0; walletIndex < walletOuts.size(); ++walletIndex) {
    for (auto& out : walletOuts[walletIndex].outs) {
      uint8_t powerOfTen = 0;
      if (m_currency.isAmountApplicableInFusionTransactionInput(out.amount, threshold, powerOfTen) && !isOutputUsed(out)) {
        allFusionReadyOuts.push_back({std::move(out), walletOuts[walletIndex].wallet});
        assert(powerOfTen < std::numeric_limits<uint64_t>::digits10 + 1);
        bucketSizes[powerOfTen]++;
      }
    }
  }

  //now, pick the bucket
  std::vector<uint8_t> bucketNumbers(bucketSizes.size());
  std::iota(bucketNumbers.begin(), bucketNumbers.end(), 0);
  std::shuffle(bucketNumbers.begin(), bucketNumbers.end(), std::default_random_engine{Crypto::rand<std::default_random_engine::result_type>()});
  size_t bucketNumberIndex = 0;
  for (; bucketNumberIndex < bucketNumbers.size(); ++bucketNumberIndex) {
    if (bucketSizes[bucketNumbers[bucketNumberIndex]] >= minInputCount) {
      break;
    }
  }
  
  if (bucketNumberIndex == bucketNumbers.size()) {
    return {};
  }

  size_t selectedBucket = bucketNumbers[bucketNumberIndex];
  assert(selectedBucket < std::numeric_limits<uint64_t>::digits10 + 1);
  assert(bucketSizes[selectedBucket] >= minInputCount);
  uint64_t lowerBound = 1;
  for (size_t i = 0; i < selectedBucket; ++i) {
    lowerBound *= 10;
  }
   
  uint64_t upperBound = selectedBucket == std::numeric_limits<uint64_t>::digits10 ? UINT64_MAX : lowerBound * 10;
  std::vector<WalletGreen::OutputToTransfer> selectedOuts;
  selectedOuts.reserve(bucketSizes[selectedBucket]);
  for (size_t outIndex = 0; outIndex < allFusionReadyOuts.size(); ++outIndex) {
    if (allFusionReadyOuts[outIndex].out.amount >= lowerBound && allFusionReadyOuts[outIndex].out.amount < upperBound) {
      selectedOuts.push_back(std::move(allFusionReadyOuts[outIndex]));
    }
  }

  assert(selectedOuts.size() >= minInputCount);

  auto outputsSortingFunction = [](const OutputToTransfer& l, const OutputToTransfer& r) { return l.out.amount < r.out.amount; };
  if (selectedOuts.size() <= maxInputCount) {
    std::sort(selectedOuts.begin(), selectedOuts.end(), outputsSortingFunction);
    return selectedOuts;
  }

  ShuffleGenerator<size_t, Crypto::random_engine<size_t>> generator(selectedOuts.size());
  std::vector<WalletGreen::OutputToTransfer> trimmedSelectedOuts;
  trimmedSelectedOuts.reserve(maxInputCount);
  for (size_t i = 0; i < maxInputCount; ++i) {
    trimmedSelectedOuts.push_back(std::move(selectedOuts[generator()]));
  }

  std::sort(trimmedSelectedOuts.begin(), trimmedSelectedOuts.end(), outputsSortingFunction);
  return trimmedSelectedOuts;  
}

} //namespace CryptoNote
