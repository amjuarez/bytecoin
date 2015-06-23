// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletHelper.h"

#include <fstream>
#include <system_error>
#include <boost/filesystem.hpp>

#include "string_tools.h"
#include "cryptonote_protocol/blobdatatype.h"

using namespace epee;

namespace cryptonote {
namespace WalletHelper {

namespace {

void openOutputFileStream(const std::string& filename, std::ofstream& file) {
  file.open(filename, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
  if (file.fail()) {
    throw std::runtime_error("error opening file: " + filename);
  }
}

}

std::error_code initAndLoadWallet(CryptoNote::IWallet& wallet, std::istream& stream, const std::string& password) {
  WalletHelper::InitWalletResultObserver initObserver;
  auto f_initError = initObserver.initResult.get_future();

  WalletHelper::IWalletRemoveObserverGuard removeGuard(wallet, initObserver);
  wallet.initAndLoad(stream, password);
  auto initError = f_initError.get();

  return initError;
}

std::error_code walletSaveWrapper(CryptoNote::IWallet& wallet, std::ostream& stream, bool saveDetailes, bool saveCache) {
  std::error_code err;
  cryptonote::WalletHelper::SaveWalletResultObserver observer;
  try {
    auto future = observer.saveResult.get_future();
    WalletHelper::IWalletRemoveObserverGuard guard(wallet, observer);
    wallet.save(stream, saveDetailes, saveCache);
    err = future.get();
  } catch (std::exception&) {
    err = make_error_code(std::errc::invalid_argument);
  }
  return err;
}

void prepareFileNames(const std::string& file_path, std::string& keys_file, std::string& wallet_file) {
  if (string_tools::get_extension(file_path) == "wallet") {
    keys_file = string_tools::cut_off_extension(file_path) + ".keys";
    wallet_file = file_path;
  } else if (string_tools::get_extension(file_path) == "keys") {
    keys_file = file_path;
    wallet_file = string_tools::cut_off_extension(file_path) + ".wallet";
  } else {
    keys_file = file_path + ".keys";
    wallet_file = file_path + ".wallet";
  }
}

void storeWallet(CryptoNote::IWallet& wallet, const std::string& walletFilename) {
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

void SendCompleteResultObserver::sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_finishedTransactions[transactionId] = result;
  m_condition.notify_one();
}

std::error_code SendCompleteResultObserver::wait(CryptoNote::TransactionId transactionId) {
  std::unique_lock<std::mutex> lock(m_mutex);
  m_condition.wait(lock, [this, &transactionId] { return m_finishedTransactions.find(transactionId) != m_finishedTransactions.end(); });
  return m_finishedTransactions.find(transactionId)->second;
}

IWalletRemoveObserverGuard::IWalletRemoveObserverGuard(CryptoNote::IWallet& wallet, CryptoNote::IWalletObserver& observer) :
  m_wallet(wallet),
  m_observer(observer),
  m_removed(false) {
  m_wallet.addObserver(&m_observer);
}

IWalletRemoveObserverGuard::~IWalletRemoveObserverGuard() {
  if (!m_removed) {
    m_wallet.removeObserver(&m_observer);
  }
}

void IWalletRemoveObserverGuard::removeObserver() {
  m_wallet.removeObserver(&m_observer);
  m_removed = true;
}

}
}
