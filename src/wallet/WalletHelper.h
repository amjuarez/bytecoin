// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <future>
#include <map>
#include <mutex>

#include "crypto/hash.h"
#include "IWallet.h"

namespace cryptonote {
namespace WalletHelper {

class SaveWalletResultObserver : public CryptoNote::IWalletObserver {
public:
  std::promise<std::error_code> saveResult;
  virtual void saveCompleted(std::error_code result) override { saveResult.set_value(result); }
};

class InitWalletResultObserver : public CryptoNote::IWalletObserver {
public:
  std::promise<std::error_code> initResult;
  virtual void initCompleted(std::error_code result) override { initResult.set_value(result); }
};

class SendCompleteResultObserver : public CryptoNote::IWalletObserver {
public:
  virtual void sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) override;

  std::error_code wait(CryptoNote::TransactionId transactionId);

private:
  std::mutex m_mutex;
  std::condition_variable m_condition;
  std::map<CryptoNote::TransactionId, std::error_code> m_finishedTransactions;
};

class IWalletRemoveObserverGuard {
public:
  IWalletRemoveObserverGuard(CryptoNote::IWallet& wallet, CryptoNote::IWalletObserver& observer);
  ~IWalletRemoveObserverGuard();

  void removeObserver();
private:
  CryptoNote::IWallet& m_wallet;
  CryptoNote::IWalletObserver& m_observer;
  bool m_removed;
};

void prepareFileNames(const std::string& file_path, std::string& keys_file, std::string& wallet_file);
void storeWallet(CryptoNote::IWallet& wallet, const std::string& walletFilename);

} }
