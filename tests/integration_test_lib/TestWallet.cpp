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

#include "TestWallet.h"

namespace Tests {
namespace Common {

using namespace CryptoNote;

const std::string TEST_PASSWORD = "password";

TestWallet::TestWallet(System::Dispatcher& dispatcher, const Currency& currency, INode& node) :
    m_dispatcher(dispatcher),
    m_synchronizationCompleted(dispatcher),
    m_someTransactionUpdated(dispatcher),
    m_currency(currency),
    m_node(node),
    m_wallet(new CryptoNote::Wallet(currency, node)),
    m_currentHeight(0) {
  m_wallet->addObserver(this);
}

TestWallet::~TestWallet() {
  m_wallet->removeObserver(this);
  // Make sure all remote spawns are executed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  m_dispatcher.yield();
}

std::error_code TestWallet::init() {
  CryptoNote::account_base walletAccount;
  walletAccount.generate();

  WalletAccountKeys walletKeys;
  walletKeys.spendPublicKey = reinterpret_cast<const WalletPublicKey&>(walletAccount.get_keys().m_account_address.m_spendPublicKey);
  walletKeys.spendSecretKey = reinterpret_cast<const WalletSecretKey&>(walletAccount.get_keys().m_spend_secret_key);
  walletKeys.viewPublicKey = reinterpret_cast<const WalletPublicKey&>(walletAccount.get_keys().m_account_address.m_viewPublicKey);
  walletKeys.viewSecretKey = reinterpret_cast<const WalletSecretKey&>(walletAccount.get_keys().m_view_secret_key);

  m_wallet->initWithKeys(walletKeys, TEST_PASSWORD);
  m_synchronizationCompleted.wait();
  return m_lastSynchronizationResult;
}

namespace {
  struct TransactionSendingWaiter : public IWalletObserver {
    System::Dispatcher& m_dispatcher;
    System::Event m_event;
    bool m_waiting = false;
    TransactionId m_expectedTxId;
    std::error_code m_result;

    TransactionSendingWaiter(System::Dispatcher& dispatcher) : m_dispatcher(dispatcher), m_event(dispatcher) {
    }

    void wait(TransactionId expectedTxId) {
      m_waiting = true;
      m_expectedTxId = expectedTxId;
      m_event.wait();
      m_waiting = false;
    }

    virtual void sendTransactionCompleted(TransactionId transactionId, std::error_code result) {
      m_dispatcher.remoteSpawn([this, transactionId, result]() {
        if (m_waiting &&  m_expectedTxId == transactionId) {
          m_result = result;
          m_event.set();
        }
      });
    }
  };
}

std::error_code TestWallet::sendTransaction(const std::string& address, uint64_t amount, TransactionHash& txHash) {
  TransactionSendingWaiter transactionSendingWaiter(m_dispatcher);
  m_wallet->addObserver(&transactionSendingWaiter);

  Transfer transfer{ address, static_cast<int64_t>(amount) };
  auto txId = m_wallet->sendTransaction(transfer, m_currency.minimumFee());
  transactionSendingWaiter.wait(txId);
  m_wallet->removeObserver(&transactionSendingWaiter);
  // TODO workaround: make sure ObserverManager doesn't have local pointers to transactionSendingWaiter, so it can be destroyed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // Run all spawned handlers from TransactionSendingWaiter::sendTransactionCompleted
  m_dispatcher.yield();

  TransactionInfo txInfo;
  if (!m_wallet->getTransaction(txId, txInfo)) {
    return std::make_error_code(std::errc::identifier_removed);
  }

  txHash = txInfo.hash;
  return transactionSendingWaiter.m_result;
}

void TestWallet::waitForSynchronizationToHeight(uint32_t height) {
  while (m_synchronizedHeight < height) {
    m_synchronizationCompleted.wait();
  }
}

IWallet* TestWallet::wallet() {
  return m_wallet.get();
}

AccountPublicAddress TestWallet::address() const {
  std::string addressString = m_wallet->getAddress();
  AccountPublicAddress address;
  bool ok = m_currency.parseAccountAddressString(addressString, address);
  assert(ok);
  return address;
}

void TestWallet::synchronizationCompleted(std::error_code result) {
  m_dispatcher.remoteSpawn([this, result]() {
    m_lastSynchronizationResult = result;
    m_synchronizedHeight = m_currentHeight;
    m_synchronizationCompleted.set();
    m_synchronizationCompleted.clear();
  });
}

void TestWallet::synchronizationProgressUpdated(uint64_t current, uint64_t total) {
  m_dispatcher.remoteSpawn([this, current]() {
    m_currentHeight = static_cast<uint32_t>(current);
  });
}

}
}
