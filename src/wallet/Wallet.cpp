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

#include "Wallet.h"
#include "wallet_errors.h"
#include "string_tools.h"
#include "serialization/binary_utils.h"
#include "storages/portable_storage_template_helper.h"
#include "WalletUtils.h"

#include <exception>
#include <algorithm>
#include <cassert>
#include <utility>
#include <thread>
#include <functional>
#include <future>

#include <time.h>
#include <string.h>

#include "WalletSerialization.h"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/lexical_cast.hpp>

namespace {

void throwNotDefined() {
  throw std::runtime_error("The behavior is not defined!");
}

bool verifyKeys(const crypto::secret_key& sec, const crypto::public_key& expected_pub) {
  crypto::public_key pub;
  bool r = crypto::secret_key_to_public_key(sec, pub);
  return r && expected_pub == pub;
}

void throwIfKeysMissmatch(const crypto::secret_key& sec, const crypto::public_key& expected_pub) {
  if (!verifyKeys(sec, expected_pub))
    throw std::system_error(make_error_code(cryptonote::error::WRONG_PASSWORD));
}

class ContextCounterHolder
{
public:
  ContextCounterHolder(CryptoNote::WalletAsyncContextCounter& shutdowner) : m_shutdowner(shutdowner) {}
  ~ContextCounterHolder() { m_shutdowner.delAsyncContext(); }

private:
  CryptoNote::WalletAsyncContextCounter& m_shutdowner;
};

template <typename F>
void runAtomic(std::mutex& mutex, F f) {
  std::unique_lock<std::mutex> lock(mutex);
  f();
}
} //namespace

namespace CryptoNote {

void Wallet::WalletNodeObserver::postponeRefresh() {
  std::unique_lock<std::mutex> lock(postponeMutex);
  postponed = true;
}

void Wallet::WalletNodeObserver::saveCompleted(std::error_code result) {
  bool startRefresh = false;
  {
    std::unique_lock<std::mutex> lock(postponeMutex);
    startRefresh = postponed;
    postponed = false;
  }

  if (startRefresh) {
    m_wallet->startRefresh();
  }
}

Wallet::Wallet(const cryptonote::Currency& currency, INode& node) :
    m_state(NOT_INITIALIZED),
    m_currency(currency),
    m_node(node),
    m_isSynchronizing(false),
    m_isStopping(false),
    m_transferDetails(currency, m_blockchain),
    m_transactionsCache(m_sendingTxsStates),
    m_synchronizer(m_account, m_node, m_blockchain, m_transferDetails, m_unconfirmedTransactions, m_transactionsCache),
    m_sender(currency, m_transactionsCache, m_sendingTxsStates, m_transferDetails, m_unconfirmedTransactions) {
  m_autoRefresher.reset(new WalletNodeObserver(this));
}

void Wallet::addObserver(IWalletObserver* observer) {
  m_observerManager.add(observer);
}

void Wallet::removeObserver(IWalletObserver* observer) {
  m_observerManager.remove(observer);
}

void Wallet::initAndGenerate(const std::string& password) {
  {
    std::unique_lock<std::mutex> stateLock(m_cacheMutex);

    if (m_state != NOT_INITIALIZED) {
      throw std::system_error(make_error_code(cryptonote::error::ALREADY_INITIALIZED));
    }

    m_node.addObserver(m_autoRefresher.get());
    addObserver(m_autoRefresher.get());

    m_account.generate();
    m_password = password;

    m_sender.init(m_account.get_keys());

    storeGenesisBlock();

    m_state = INITIALIZED;
  }

  m_observerManager.notify(&IWalletObserver::initCompleted, std::error_code());
  refresh();
}

void Wallet::initWithKeys(const AccountKeys& accountKeys, const std::string& password) {
  {
    std::unique_lock<std::mutex> stateLock(m_cacheMutex);

    if (m_state != NOT_INITIALIZED) {
      throw std::system_error(make_error_code(cryptonote::error::ALREADY_INITIALIZED));
    }

    m_node.addObserver(m_autoRefresher.get());
    addObserver(m_autoRefresher.get());

    cryptonote::account_keys keys;

    std::copy(accountKeys.spendPublicKey.begin(),
        accountKeys.spendPublicKey.end(),
        reinterpret_cast<uint8_t *>(&keys.m_account_address.m_spendPublicKey));

    std::copy(accountKeys.viewPublicKey.begin(),
        accountKeys.viewPublicKey.end(),
        reinterpret_cast<uint8_t *>(&keys.m_account_address.m_viewPublicKey));

    std::copy(accountKeys.spendSecretKey.begin(),
        accountKeys.spendSecretKey.end(),
        reinterpret_cast<uint8_t *>(&keys.m_spend_secret_key));

    std::copy(accountKeys.viewSecretKey.begin(),
        accountKeys.viewSecretKey.end(),
        reinterpret_cast<uint8_t *>(&keys.m_view_secret_key));

    m_account.set_keys(keys);
    m_account.set_createtime(0);

    m_password = password;

    m_sender.init(m_account.get_keys());

    storeGenesisBlock();

    m_state = INITIALIZED;
  }

  m_observerManager.notify(&IWalletObserver::initCompleted, std::error_code());
  refresh();
}

void Wallet::storeGenesisBlock() {
  m_blockchain.push_back(m_currency.genesisBlockHash());
}

void Wallet::initAndLoad(std::istream& source, const std::string& password) {
  std::unique_lock<std::mutex> stateLock(m_cacheMutex);

  if (m_state != NOT_INITIALIZED) {
    throw std::system_error(make_error_code(cryptonote::error::NOT_INITIALIZED));
  }

  m_node.addObserver(m_autoRefresher.get());
  addObserver(m_autoRefresher.get());

  m_password = password;
  m_state = LOADING;

  std::thread loader(&Wallet::doLoad, this, std::ref(source));
  loader.detach();
}

void Wallet::doLoad(std::istream& source) {
  try
  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);

    boost::archive::binary_iarchive ar(source);

    crypto::chacha_iv iv;
    std::string chacha_str;;
    ar >> chacha_str;

    ::serialization::parse_binary(chacha_str, iv);

    std::string cipher;
    ar >> cipher;

    std::string plain;
    decrypt(cipher, plain, iv, m_password);

    std::stringstream restore(plain);

    try
    {
      //boost archive ctor throws an exception if password is wrong (i.e. there's garbage in a stream)
      boost::archive::binary_iarchive dataArchive(restore);

      dataArchive >> m_account;

      throwIfKeysMissmatch(m_account.get_keys().m_view_secret_key, m_account.get_keys().m_account_address.m_viewPublicKey);
      throwIfKeysMissmatch(m_account.get_keys().m_spend_secret_key, m_account.get_keys().m_account_address.m_spendPublicKey);

      dataArchive >> m_blockchain;

      m_transferDetails.load(dataArchive);
      m_unconfirmedTransactions.load(dataArchive);
      m_transactionsCache.load(dataArchive);

      m_unconfirmedTransactions.synchronizeTransactionIds(m_transactionsCache);
    }
    catch (std::exception&) {
      throw std::system_error(make_error_code(cryptonote::error::WRONG_PASSWORD));
    }

    if (m_blockchain.empty()) {
      storeGenesisBlock();
    }
    m_sender.init(m_account.get_keys());
  }
  catch (std::system_error& e) {
    runAtomic(m_cacheMutex, [this] () {this->m_state = Wallet::NOT_INITIALIZED;} );
    m_observerManager.notify(&IWalletObserver::initCompleted, e.code());
    return;
  }
  catch (std::exception&) {
    runAtomic(m_cacheMutex, [this] () {this->m_state = Wallet::NOT_INITIALIZED;} );
    m_observerManager.notify(&IWalletObserver::initCompleted, make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR));
    return;
  }

  runAtomic(m_cacheMutex, [this] () {this->m_state = Wallet::INITIALIZED;} );

  m_observerManager.notify(&IWalletObserver::initCompleted, std::error_code());

  refresh();
}

void Wallet::decrypt(const std::string& cipher, std::string& plain, crypto::chacha_iv iv, const std::string& password) {
  crypto::chacha_key key;
  crypto::cn_context context;
  crypto::generate_chacha8_key(context, password, key);

  plain.resize(cipher.size());

  crypto::chacha8(cipher.data(), cipher.size(), key, iv, &plain[0]);
}

void Wallet::shutdown() {
  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);

    if (m_isStopping)
      throwNotDefined();

    m_isStopping = true;

    if (m_state == NOT_INITIALIZED)
      throwNotDefined();

    m_sender.stop();
    m_synchronizer.stop();
  }

  m_asyncContextCounter.waitAsyncContextsFinish();
  m_node.removeObserver(m_autoRefresher.get());
  removeObserver(m_autoRefresher.get());
}

void Wallet::save(std::ostream& destination, bool saveDetailed, bool saveCache) {
  if(m_isStopping) {
    m_observerManager.notify(&IWalletObserver::saveCompleted, make_error_code(cryptonote::error::OPERATION_CANCELLED));
    return;
  }

  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);

    throwIf(m_state != INITIALIZED, cryptonote::error::WRONG_STATE);

    m_state = SAVING;
  }

  m_asyncContextCounter.addAsyncContext();
  std::thread saver(&Wallet::doSave, this, std::ref(destination), saveDetailed, saveCache);
  saver.detach();
}

void Wallet::doSave(std::ostream& destination, bool saveDetailed, bool saveCache) {
  ContextCounterHolder counterHolder(m_asyncContextCounter);

  try {
    //TODO: exception safety: leave destination stream empty in case of errors
    boost::archive::binary_oarchive ar(destination);

    std::stringstream original;
    std::unique_lock<std::mutex> lock(m_cacheMutex);

    boost::archive::binary_oarchive archive(original);

    archive << m_account;

    const BlockchainContainer& blockchain = saveCache ? m_blockchain : BlockchainContainer();

    archive << blockchain;

    m_transferDetails.save(archive, saveCache);
    m_unconfirmedTransactions.save(archive, saveCache);
    m_transactionsCache.save(archive, saveDetailed, saveCache);

    std::string plain = original.str();
    std::string cipher;

    crypto::chacha_iv iv = encrypt(plain, cipher);

    std::string chacha_str;
    ::serialization::dump_binary(iv, chacha_str);
    ar << chacha_str;
    ar << cipher;

    m_state = INITIALIZED;
  }
  catch (std::system_error& e) {
    runAtomic(m_cacheMutex, [this] () {this->m_state = Wallet::INITIALIZED;} );
    m_observerManager.notify(&IWalletObserver::saveCompleted, e.code());
    return;
  }
  catch (std::exception&) {
    runAtomic(m_cacheMutex, [this] () {this->m_state = Wallet::INITIALIZED;} );
    m_observerManager.notify(&IWalletObserver::saveCompleted, make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR));
    return;
  }

  m_observerManager.notify(&IWalletObserver::saveCompleted, std::error_code());
}

crypto::chacha_iv Wallet::encrypt(const std::string& plain, std::string& cipher) {
  crypto::chacha_key key;
  crypto::cn_context context;
  crypto::generate_chacha8_key(context, m_password, key);

  cipher.resize(plain.size());

  crypto::chacha_iv iv = crypto::rand<crypto::chacha_iv>();
  crypto::chacha8(plain.data(), plain.size(), key, iv, &cipher[0]);

  return iv;
}

std::error_code Wallet::changePassword(const std::string& oldPassword, const std::string& newPassword) {
  std::unique_lock<std::mutex> passLock(m_cacheMutex);

  throwIfNotInitialised();

  if (m_password.compare(oldPassword))
    return make_error_code(cryptonote::error::WRONG_PASSWORD);

  //we don't let the user to change the password while saving
  m_password = newPassword;

  return std::error_code();
}

std::string Wallet::getAddress() {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_currency.accountAddressAsString(m_account);
}

uint64_t Wallet::actualBalance() {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transferDetails.countActualBalance();
}

uint64_t Wallet::pendingBalance() {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return doPendingBalance();
}

uint64_t Wallet::doPendingBalance() {
  uint64_t amount = 0;
  amount = m_transferDetails.countPendingBalance();
  amount += m_unconfirmedTransactions.countPendingBalance();

  return amount;
}

size_t Wallet::getTransactionCount() {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transactionsCache.getTransactionCount();
}

size_t Wallet::getTransferCount() {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transactionsCache.getTransferCount();
}

TransactionId Wallet::findTransactionByTransferId(TransferId transferId) {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transactionsCache.findTransactionByTransferId(transferId);
}

bool Wallet::getTransaction(TransactionId transactionId, TransactionInfo& transaction) {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transactionsCache.getTransaction(transactionId, transaction);
}

bool Wallet::getTransfer(TransferId transferId, Transfer& transfer) {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transactionsCache.getTransfer(transferId, transfer);
}

TransactionId Wallet::sendTransaction(const Transfer& transfer, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp, const std::vector<TransactionMessage>& messages) {
  std::vector<Transfer> transfers;
  transfers.push_back(transfer);

  return sendTransaction(transfers, fee, extra, mixIn, unlockTimestamp, messages);
}

TransactionId Wallet::sendTransaction(const std::vector<Transfer>& transfers, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp, const std::vector<TransactionMessage>& messages) {
  TransactionId txId = 0;
  std::shared_ptr<WalletRequest> request;
  std::deque<std::shared_ptr<WalletEvent> > events;

  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    request = m_sender.makeSendRequest(txId, events, transfers, fee, extra, mixIn, unlockTimestamp, messages);
  }

  notifyClients(events);

  if (request) {
    m_asyncContextCounter.addAsyncContext();
    request->perform(m_node, std::bind(&Wallet::sendTransactionCallback, this, std::placeholders::_1, std::placeholders::_2));
  }

  return txId;
}

void Wallet::sendTransactionCallback(WalletRequest::Callback callback, std::error_code ec) {
  ContextCounterHolder counterHolder(m_asyncContextCounter);
  std::deque<std::shared_ptr<WalletEvent> > events;

  boost::optional<std::shared_ptr<WalletRequest> > nextRequest;
  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    callback(events, nextRequest, ec);
  }

  notifyClients(events);

  if (nextRequest) {
    m_asyncContextCounter.addAsyncContext();
    (*nextRequest)->perform(m_node, std::bind(&Wallet::synchronizationCallback, this, std::placeholders::_1, std::placeholders::_2));
  }

}

void Wallet::synchronizationCallback(WalletRequest::Callback callback, std::error_code ec) {
  ContextCounterHolder counterHolder(m_asyncContextCounter);

  std::deque<std::shared_ptr<WalletEvent> > events;
  boost::optional<std::shared_ptr<WalletRequest> > nextRequest;
  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    callback(events, nextRequest, ec);

    if (!nextRequest)
      m_isSynchronizing = false;
  }

  notifyClients(events);

  if (nextRequest) {
    m_asyncContextCounter.addAsyncContext();
    (*nextRequest)->perform(m_node, std::bind(&Wallet::synchronizationCallback, this, std::placeholders::_1, std::placeholders::_2));
  }
}

std::error_code Wallet::cancelTransaction(size_t transactionId) {
  return make_error_code(cryptonote::error::TX_CANCEL_IMPOSSIBLE);
}

void Wallet::throwIfNotInitialised() {
  if (m_state == NOT_INITIALIZED || m_state == LOADING)
    throw std::system_error(make_error_code(cryptonote::error::NOT_INITIALIZED));
}

void Wallet::startRefresh() {
  refresh();
}

void Wallet::refresh() {
  if (m_isStopping)
    return;

  std::shared_ptr<WalletRequest> req;
  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);

    if (m_state == SAVING) {
      m_autoRefresher->postponeRefresh();
      return;
    }

    if (m_state != INITIALIZED) {
      return;
    }

    if (m_isSynchronizing) {
      return;
    }

    m_isSynchronizing = true;

    req = m_synchronizer.makeStartRefreshRequest();
  }

  m_asyncContextCounter.addAsyncContext();
  req->perform(m_node, std::bind(&Wallet::synchronizationCallback, this, std::placeholders::_1, std::placeholders::_2));
}

void Wallet::notifyClients(std::deque<std::shared_ptr<WalletEvent> >& events) {
  while (!events.empty()) {
    std::shared_ptr<WalletEvent> event = events.front();
    event->notify(m_observerManager);
    events.pop_front();
  }
}

void Wallet::getAccountKeys(AccountKeys& keys) {
  if (m_state == NOT_INITIALIZED) {
    throw std::system_error(make_error_code(cryptonote::error::NOT_INITIALIZED));
  }

  const cryptonote::account_keys& accountKeys = m_account.get_keys();
  std::copy(reinterpret_cast<const uint8_t*>(&accountKeys.m_account_address.m_spendPublicKey),
      reinterpret_cast<const uint8_t*>(&accountKeys.m_account_address.m_spendPublicKey) + sizeof(crypto::public_key),
      keys.spendPublicKey.begin());

  std::copy(reinterpret_cast<const uint8_t*>(&accountKeys.m_spend_secret_key),
      reinterpret_cast<const uint8_t*>(&accountKeys.m_spend_secret_key) + sizeof(crypto::secret_key),
      keys.spendSecretKey.begin());

  std::copy(reinterpret_cast<const uint8_t*>(&accountKeys.m_account_address.m_viewPublicKey),
      reinterpret_cast<const uint8_t*>(&accountKeys.m_account_address.m_viewPublicKey) + sizeof(crypto::public_key),
      keys.viewPublicKey.begin());

  std::copy(reinterpret_cast<const uint8_t*>(&accountKeys.m_view_secret_key),
      reinterpret_cast<const uint8_t*>(&accountKeys.m_view_secret_key) + sizeof(crypto::secret_key),
      keys.viewSecretKey.begin());
}

} //namespace CryptoNote
