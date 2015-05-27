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

#include "WalletService.h"

#include "WalletServiceErrorCodes.h"
#include "JsonRpcMessages.h"
#include "WalletFactory.h"
#include "NodeFactory.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "crypto/crypto.h"
#include "wallet/LegacyKeysImporter.h"
#include "Common/util.h"

#include <future>
#include <assert.h>
#include <sstream>
#include <unordered_set>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <stdio.h>
#endif

namespace {

void addPaymentIdToExtra(const std::string& paymentId, std::string& extra) {
  std::vector<uint8_t> extraVector;
  if (!CryptoNote::createTxExtraWithPaymentId(paymentId, extraVector)) {
    throw std::runtime_error("Couldn't add payment id to extra");
  }

  std::copy(extraVector.begin(), extraVector.end(), std::back_inserter(extra));
}

bool checkPaymentId(const std::string& paymentId) {
  if (paymentId.size() != 64) {
    return false;
  }

  return std::all_of(paymentId.begin(), paymentId.end(), [] (const char c) {
    if (c >= '0' && c <= '9') {
      return true;
    }

    if (c >= 'a' && c <= 'f') {
      return true;
    }

    if (c >= 'A' && c <= 'F') {
      return true;
    }

    return false;
  });
}

bool createOutputBinaryFile(const std::string& filename, std::fstream& file) {
  file.open(filename.c_str(), std::fstream::in | std::fstream::out | std::ofstream::binary);
  if (file) {
    file.close();
    return false;
  }

  file.open(filename.c_str(), std::fstream::out | std::fstream::binary);
  return true;
}

std::string createTemporaryFile(const std::string& path, std::fstream& tempFile) {
  bool created = false;
  std::string temporaryName;

  for (size_t i = 1; i < 100; i++) {
    temporaryName = path + "." + std::to_string(i++);

    if (createOutputBinaryFile(temporaryName, tempFile)) {
      created = true;
      break;
    }
  }

  if (!created) {
    throw std::runtime_error("Couldn't create temporary file: " + temporaryName);
  }

  return temporaryName;
}

//returns true on success
bool deleteFile(const std::string& filename) {
#ifdef WIN32
  return DeleteFile(filename.c_str()) != 0;
#else
  return unlink(filename.c_str()) == 0;
#endif
}

void replaceWalletFiles(const std::string &path, const std::string &tempFilePath) {
  tools::replace_file(tempFilePath, path);
}

}

namespace PaymentService {

void createWalletFile(std::fstream& walletFile, const std::string& filename) {
  walletFile.open(filename.c_str(), std::fstream::in | std::fstream::out | std::fstream::binary);
  if (walletFile) {
    walletFile.close();
    throw std::runtime_error("Wallet file already exists");
  }

  walletFile.open(filename.c_str(), std::fstream::out);
  walletFile.close();

  walletFile.open(filename.c_str(), std::fstream::in | std::fstream::out | std::fstream::binary);
}

void saveWallet(CryptoNote::IWallet* wallet, std::fstream& walletFile, bool saveDetailed = true, bool saveCache = true) {
  WalletSaveObserver saveObserver;
  wallet->addObserver(&saveObserver);
  wallet->save(walletFile, saveDetailed, saveCache);
  saveObserver.waitForSaveEnd();
  wallet->removeObserver(&saveObserver);

  walletFile.flush();
}

void secureSaveWallet(CryptoNote::IWallet* wallet, const std::string& path, bool saveDetailed = true, bool saveCache = true) {
  std::fstream tempFile;
  std::string tempFilePath = createTemporaryFile(path, tempFile);

  try {
    saveWallet(wallet, tempFile, saveDetailed, saveCache);
  } catch (std::exception&) {
    deleteFile(tempFilePath);
    tempFile.close();
    throw;
  }
  tempFile.close();

  replaceWalletFiles(path, tempFilePath);
}

void generateNewWallet(CryptoNote::Currency &currency, const Configuration &conf, Logging::ILogger& logger) {
  Logging::LoggerRef log(logger, "generateNewWallet");

  CryptoNote::INode* nodeStub = NodeFactory::createNodeStub();
  std::unique_ptr<CryptoNote::INode> nodeGuard(nodeStub);

  CryptoNote::IWallet* wallet = WalletFactory::createWallet(currency, *nodeStub);
  std::unique_ptr<CryptoNote::IWallet> walletGuard(wallet);

  log(Logging::INFO) << "Generating new wallet";

  std::fstream walletFile;
  createWalletFile(walletFile, conf.walletFile);

  WalletLoadObserver loadObserver;
  wallet->addObserver(&loadObserver);

  wallet->initAndGenerate(conf.walletPassword);

  loadObserver.waitForLoadEnd();
  wallet->removeObserver(&loadObserver);

  log(Logging::INFO) << "New wallet is generated. Address: " << wallet->getAddress();

  saveWallet(wallet, walletFile, false, false);
  log(Logging::INFO) << "Wallet is saved";
}

void importLegacyKeys(const Configuration& conf) {
  std::stringstream archive;

  CryptoNote::importLegacyKeys(conf.importKeys, conf.walletPassword, archive);

  std::fstream walletFile;
  createWalletFile(walletFile, conf.walletFile);

  archive.flush();
  walletFile << archive.rdbuf();
  walletFile.flush();
}

WalletService::WalletService(const CryptoNote::Currency& currency, System::Dispatcher& sys, CryptoNote::INode& node,
  const Configuration& conf, Logging::ILogger& logger) :
    config(conf),
    inited(false),
    sendObserver(sys),
    logger(logger, "WaleltService"),
    txIdIndex(boost::get<0>(paymentsCache)),
    paymentIdIndex(boost::get<1>(paymentsCache))
{
  wallet.reset(WalletFactory::createWallet(currency, node));
}

WalletService::~WalletService() {
  if (wallet) {
    if (inited) {
      wallet->removeObserver(&sendObserver);
      wallet->removeObserver(this);
      wallet->shutdown();
    }
  }
}

void WalletService::init() {
  loadWallet();
  loadPaymentsCache();

  wallet->addObserver(&sendObserver);
  wallet->addObserver(this);

  inited = true;
}

void WalletService::saveWallet() {
  PaymentService::secureSaveWallet(wallet.get(), config.walletFile, true, true);
  logger(Logging::INFO) << "Wallet is saved";
}

void WalletService::loadWallet() {
  std::ifstream inputWalletFile;
  inputWalletFile.open(config.walletFile.c_str(), std::fstream::in | std::fstream::binary);
  if (!inputWalletFile) {
    throw std::runtime_error("Couldn't open wallet file");
  }

  logger(Logging::INFO) << "Loading wallet";

  WalletLoadObserver loadObserver;
  wallet->addObserver(&loadObserver);

  wallet->initAndLoad(inputWalletFile, config.walletPassword);

  loadObserver.waitForLoadEnd();

  wallet->removeObserver(&loadObserver);

  logger(Logging::INFO) << "Wallet loading is finished. Address: " << wallet->getAddress();
}

void WalletService::loadPaymentsCache() {
  size_t txCount = wallet->getTransactionCount();

  logger(Logging::DEBUGGING) << "seeking for payments among " << txCount << " transactions";

  for (size_t id = 0; id < txCount; ++id) {
    CryptoNote::TransactionInfo tx;
    if (!wallet->getTransaction(id, tx)) {
      logger(Logging::DEBUGGING) << "tx " << id << " doesn't exist";
      continue;
    }

    if (tx.totalAmount < 0) {
      logger(Logging::DEBUGGING) << "tx " << id << " has negative amount";
      continue;
    }

    std::vector<uint8_t> extraVector(tx.extra.begin(), tx.extra.end());

    crypto::hash paymentId;
    if (!CryptoNote::getPaymentIdFromTxExtra(extraVector, paymentId)) {
      logger(Logging::DEBUGGING) << "tx " << id << " has no payment id";
      continue;
    }

    logger(Logging::DEBUGGING) << "transaction " << id << " has been inserted with payment id " << paymentId;
    insertTransaction(id, paymentId);
  }
}

std::error_code WalletService::sendTransaction(const SendTransactionRequest& req, SendTransactionResponse& resp) {
  assert(wallet);
  logger(Logging::DEBUGGING) << "Send transaction request came";

  try {
    std::vector<CryptoNote::Transfer> transfers;
    makeTransfers(req.destinations, transfers);

    std::string extra;
    if (!req.paymentId.empty()) {
      addPaymentIdToExtra(req.paymentId, extra);
    }

    CryptoNote::TransactionId txId = wallet->sendTransaction(transfers, req.fee, extra, req.mixin, req.unlockTime);
    if (txId == CryptoNote::INVALID_TRANSACTION_ID) {
      logger(Logging::WARNING) << "Unable to send transaction";
      throw std::runtime_error("Error occured while sending transaction");
    }

    std::error_code ec;
    sendObserver.waitForTransactionFinished(txId, ec);

    if (ec) {
      return ec;
    }

    resp.transactionId = txId;
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Error while sending transaction: " << x.what();
    return x.code();
  }

  return std::error_code();
}

void WalletService::makeTransfers(const std::vector<PaymentService::TransferDestination>& destinations, std::vector<CryptoNote::Transfer>& transfers) {
  transfers.reserve(destinations.size());

  for (auto dest: destinations) {
    transfers.push_back( { dest.address, static_cast<int64_t>(dest.amount) } );
  }
}

std::error_code WalletService::getAddress(std::string& address) {
  logger(Logging::DEBUGGING) << "Get address request came";

  try {
    address = wallet->getAddress();
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Error while getting address: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getActualBalance(uint64_t& actualBalance) {
  logger(Logging::DEBUGGING) << "Get actual balance request came";

  try {
    actualBalance = wallet->actualBalance();
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get actual balance: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getPendingBalance(uint64_t& pendingBalance) {
  logger(Logging::DEBUGGING) << "Get pending balance request came";

  try {
    pendingBalance = wallet->pendingBalance();
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get pending balance: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getTransactionsCount(uint64_t& txCount) {
  logger(Logging::DEBUGGING) << "Get get transactions count request came";

  try {
    txCount = wallet->getTransactionCount();
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get transactions count: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getTransfersCount(uint64_t& trCount) {
  logger(Logging::DEBUGGING) << "Get get transfers count request came";

  try {
    trCount = wallet->getTransferCount();
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get transfers count: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getTransactionByTransferId(CryptoNote::TransferId transfer, CryptoNote::TransactionId& transaction) {
  logger(Logging::DEBUGGING) << "getTransactionByTransferId request came";

  try {
    transaction = wallet->findTransactionByTransferId(transfer);
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get transaction id by transfer id count: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getTransaction(CryptoNote::TransactionId txId, bool& found, TransactionRpcInfo& rpcInfo) {
  logger(Logging::DEBUGGING) << "getTransaction request came";

  try {
    CryptoNote::TransactionInfo txInfo;

    found = wallet->getTransaction(txId, txInfo);
    if (!found) {
      return std::error_code();
    }

    fillTransactionRpcInfo(txInfo, rpcInfo);
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get transaction: " << x.what();
    return x.code();
  }

  return std::error_code();
}

void WalletService::fillTransactionRpcInfo(const CryptoNote::TransactionInfo& txInfo, TransactionRpcInfo& rpcInfo) {
  rpcInfo.firstTransferId = txInfo.firstTransferId;
  rpcInfo.transferCount = txInfo.transferCount;
  rpcInfo.totalAmount = txInfo.totalAmount;
  rpcInfo.fee = txInfo.fee;
  rpcInfo.isCoinbase = txInfo.isCoinbase;
  rpcInfo.blockHeight = txInfo.blockHeight;
  rpcInfo.timestamp = txInfo.timestamp;
  rpcInfo.extra = Common::toHex(txInfo.extra.data(), txInfo.extra.size());
  rpcInfo.hash = Common::podToHex(txInfo.hash);
}

std::error_code WalletService::getTransfer(CryptoNote::TransferId txId, bool& found, TransferRpcInfo& rpcInfo) {
  logger(Logging::DEBUGGING) << "getTransfer request came";

  try {
    CryptoNote::Transfer transfer;

    found = wallet->getTransfer(txId, transfer);
    if (!found) {
      return std::error_code();
    }

    fillTransferRpcInfo(transfer, rpcInfo);
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get transfer: " << x.what();
    return x.code();
  }

  return std::error_code();
}

void WalletService::fillTransferRpcInfo(const CryptoNote::Transfer& transfer, TransferRpcInfo& rpcInfo) {
  rpcInfo.address = transfer.address;
  rpcInfo.amount = transfer.amount;
}

std::error_code WalletService::getIncomingPayments(const std::vector<std::string>& payments, IncomingPayments& result) {
  logger(Logging::DEBUGGING) << "getIncomingPayments request came";

  for (const std::string& payment: payments) {
    if (!checkPaymentId(payment)) {
      return make_error_code(error::REQUEST_ERROR);
    }

    std::string paymentString = payment;
    std::transform(paymentString.begin(), paymentString.end(), paymentString.begin(), ::tolower);

    auto pair = paymentIdIndex.equal_range(paymentString);

    for (auto it = pair.first; it != pair.second; ++it) {
      CryptoNote::TransactionInfo tx;
      if (!wallet->getTransaction(it->transactionId, tx)) {
        continue;
      }

      std::string hashString = Common::podToHex(tx.hash);

      PaymentDetails details;
      details.txHash = std::move(hashString);
      details.amount = static_cast<uint64_t>(tx.totalAmount);
      details.blockHeight = tx.blockHeight;
      details.unlockTime = 0; //TODO: this is stub. fix it when wallet api allows to retrieve it

      result[it->paymentId].push_back(std::move(details));
    }
  }

  return std::error_code();
}

void WalletService::externalTransactionCreated(CryptoNote::TransactionId transactionId) {
  logger(Logging::DEBUGGING) << "external transaction created " << transactionId;
  CryptoNote::TransactionInfo tx;
  if (!wallet->getTransaction(transactionId, tx)) {
    return;
  }

  if (tx.totalAmount < 0) {
    return;
  }

  logger(Logging::DEBUGGING) << "external transaction created " << transactionId << " extra size: " << tx.extra.size();
  std::vector<uint8_t> extraVector(tx.extra.begin(), tx.extra.end());
  crypto::hash paymentId;
  if (!CryptoNote::getPaymentIdFromTxExtra(extraVector, paymentId)) {
    logger(Logging::DEBUGGING) << "transaction " << transactionId << " has no payment id";
    return;
  }

  insertTransaction(transactionId, paymentId);

  logger(Logging::DEBUGGING) << "transaction " << transactionId << " has been added to payments cache";
}

void WalletService::transactionUpdated(CryptoNote::TransactionId transactionId) {
  CryptoNote::TransactionInfo tx;
  if (!wallet->getTransaction(transactionId, tx)) {
    return;
  }

  if (tx.totalAmount < 0) {
    return;
  }

  if (tx.blockHeight != CryptoNote::UNCONFIRMED_TRANSACTION_HEIGHT) {
    auto it = txIdIndex.find(transactionId);
    if (it != txIdIndex.end()) {
      return;
    }

    //insert confirmed transaction
    std::vector<uint8_t> extraVector(tx.extra.begin(), tx.extra.end());
    crypto::hash paymentId;
    if (!CryptoNote::getPaymentIdFromTxExtra(extraVector, paymentId)) {
      logger(Logging::DEBUGGING) << "transaction " << transactionId << " has no payment id";
      return;
    }

    insertTransaction(transactionId, paymentId);
    logger(Logging::DEBUGGING) << "transaction " << transactionId << " has been inserted to payments cache";
  } else {
    auto it = txIdIndex.find(transactionId);
    if (it != txIdIndex.end()) {
      txIdIndex.erase(it);
      logger(Logging::DEBUGGING) << "transaction " << transactionId << " has been erased from payments cache";
    }
  }
}

void WalletService::insertTransaction(CryptoNote::TransactionId id, const crypto::hash& paymentIdBin) {
  paymentsCache.insert(PaymentItem{ Common::podToHex(paymentIdBin), id });
}

} //namespace PaymentService
