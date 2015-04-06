#pragma once

#include <future>

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
  std::future<CryptoNote::TransactionId> expectedTxID;
  std::promise<std::error_code> sendResult;
  virtual void sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) override {
    if (transactionId == expectedTxID.get()) sendResult.set_value(result);
  }
};

void prepareFileNames(const std::string& file_path, std::string& keys_file, std::string& wallet_file);

} }
