// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletHelper.h"
#include "Common/PathTools.h"

#include <fstream>
#include <boost/filesystem.hpp>

using namespace CryptoNote;

namespace {

void openOutputFileStream(const std::string& filename, std::ofstream& file) {
  file.open(filename, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
  if (file.fail()) {
    throw std::runtime_error("error opening file: " + filename);
  }
}

std::error_code walletSaveWrapper(CryptoNote::IWalletLegacy& wallet, std::ofstream& file, bool saveDetailes, bool saveCache) {
  CryptoNote::WalletHelper::SaveWalletResultObserver o;

  std::error_code e;
  try {
    std::future<std::error_code> f = o.saveResult.get_future();
    wallet.addObserver(&o);
    wallet.save(file, saveDetailes, saveCache);
    e = f.get();
  } catch (std::exception&) {
    wallet.removeObserver(&o);
    return make_error_code(std::errc::invalid_argument);
  }

  wallet.removeObserver(&o);
  return e;
}

}

void WalletHelper::prepareFileNames(const std::string& file_path, std::string& keys_file, std::string& wallet_file) {
  if (Common::GetExtension(file_path) == ".wallet") {
    keys_file = Common::RemoveExtension(file_path) + ".keys";
    wallet_file = file_path;
  } else if (Common::GetExtension(file_path) == ".keys") {
    keys_file = file_path;
    wallet_file = Common::RemoveExtension(file_path) + ".wallet";
  } else {
    keys_file = file_path + ".keys";
    wallet_file = file_path + ".wallet";
  }
}

void WalletHelper::SendCompleteResultObserver::sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_finishedTransactions[transactionId] = result;
  m_condition.notify_one();
}

std::error_code WalletHelper::SendCompleteResultObserver::wait(CryptoNote::TransactionId transactionId) {
  std::unique_lock<std::mutex> lock(m_mutex);

  m_condition.wait(lock, [this, &transactionId] {
    auto it = m_finishedTransactions.find(transactionId);
    if (it == m_finishedTransactions.end()) {
      return false;
    }

    m_result = it->second;
    return true;
  });

  return m_result;
}

WalletHelper::IWalletRemoveObserverGuard::IWalletRemoveObserverGuard(CryptoNote::IWalletLegacy& wallet, CryptoNote::IWalletLegacyObserver& observer) :
  m_wallet(wallet),
  m_observer(observer),
  m_removed(false) {
  m_wallet.addObserver(&m_observer);
}

WalletHelper::IWalletRemoveObserverGuard::~IWalletRemoveObserverGuard() {
  if (!m_removed) {
    m_wallet.removeObserver(&m_observer);
  }
}

void WalletHelper::IWalletRemoveObserverGuard::removeObserver() {
  m_wallet.removeObserver(&m_observer);
  m_removed = true;
}

void WalletHelper::storeWallet(CryptoNote::IWalletLegacy& wallet, const std::string& walletFilename) {
  boost::filesystem::path tempFile = boost::filesystem::unique_path(walletFilename + ".tmp.%%%%-%%%%");

  if (boost::filesystem::exists(walletFilename)) {
    boost::filesystem::rename(walletFilename, tempFile);
  }

  std::ofstream file;
  try {
    openOutputFileStream(walletFilename, file);
  } catch (std::exception&) {
    if (boost::filesystem::exists(tempFile)) {
      boost::filesystem::rename(tempFile, walletFilename);
    }
    throw;
  }

  std::error_code saveError = walletSaveWrapper(wallet, file, true, true);
  if (saveError) {
    file.close();
    boost::filesystem::remove(walletFilename);
    boost::filesystem::rename(tempFile, walletFilename);
    throw std::system_error(saveError);
  }

  file.close();

  boost::system::error_code ignore;
  boost::filesystem::remove(tempFile, ignore);
}
