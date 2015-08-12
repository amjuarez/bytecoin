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

#include "WalletService.h"


#include <future>
#include <assert.h>
#include <sstream>
#include <unordered_set>

#include <boost/filesystem/operations.hpp>

#include <System/Timer.h>
#include <System/InterruptedException.h>
#include "Common/Util.h"

#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/TransactionExtra.h"

#include "PaymentServiceJsonRpcMessages.h"
#include "WalletFactory.h"
#include "NodeFactory.h"

#include "Wallet/LegacyKeysImporter.h"

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
  boost::system::error_code err;
  return boost::filesystem::remove(filename, err) && !err;
}

void replaceWalletFiles(const std::string &path, const std::string &tempFilePath) {
  Tools::replace_file(tempFilePath, path);
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
  wallet->save(walletFile, saveDetailed, saveCache);
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

void generateNewWallet(const CryptoNote::Currency &currency, const WalletConfiguration &conf, Logging::ILogger& logger, System::Dispatcher& dispatcher) {
  Logging::LoggerRef log(logger, "generateNewWallet");

  CryptoNote::INode* nodeStub = NodeFactory::createNodeStub();
  std::unique_ptr<CryptoNote::INode> nodeGuard(nodeStub);

  CryptoNote::IWallet* wallet = WalletFactory::createWallet(currency, *nodeStub, dispatcher);
  std::unique_ptr<CryptoNote::IWallet> walletGuard(wallet);

  log(Logging::INFO) << "Generating new wallet";

  std::fstream walletFile;
  createWalletFile(walletFile, conf.walletFile);

  wallet->initialize(conf.walletPassword);
  auto address = wallet->createAddress();

  log(Logging::INFO) << "New wallet is generated. Address: " << address;

  saveWallet(wallet, walletFile, false, false);
  log(Logging::INFO) << "Wallet is saved";
}

void importLegacyKeys(const std::string &legacyKeysFile, const WalletConfiguration &conf) {
  std::stringstream archive;

  CryptoNote::importLegacyKeys(legacyKeysFile, conf.walletPassword, archive);

  std::fstream walletFile;
  createWalletFile(walletFile, conf.walletFile);

  archive.flush();
  walletFile << archive.rdbuf();
  walletFile.flush();
}

WalletService::WalletService(const CryptoNote::Currency& currency, System::Dispatcher& sys, CryptoNote::INode& node,
  const WalletConfiguration& conf, Logging::ILogger& logger) :
    config(conf),
    inited(false),
    logger(logger, "WalletService"),
    txIdIndex(boost::get<0>(paymentsCache)),
    paymentIdIndex(boost::get<1>(paymentsCache)),
    dispatcher(sys),
    refreshContext(dispatcher)
{
  wallet.reset(WalletFactory::createWallet(currency, node, dispatcher));
}

WalletService::~WalletService() {
  if (wallet) {
    if (inited) {
      wallet->stop();
      refreshContext.wait();
      wallet->shutdown();
    }
  }
}

void WalletService::init() {
  loadWallet();
  loadPaymentsCacheAndTransferIndices();
  refreshContext.spawn([this] { refresh(); });

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

  wallet->load(inputWalletFile, config.walletPassword);

  logger(Logging::INFO) << "Wallet loading is finished.";
}

void WalletService::loadPaymentsCacheAndTransferIndices() {
  size_t txCount = wallet->getTransactionCount();
  transfersIndices.resize(1);
  transfersIndices[0] = 0;

  logger(Logging::DEBUGGING) << "seeking for payments among " << txCount << " transactions";

  for (size_t id = 0; id < txCount; ++id) {
    CryptoNote::WalletTransaction tx = wallet->getTransaction(id);

    transfersIndices.push_back(transfersIndices[id] + wallet->getTransactionTransferCount(id));    
    
    if (tx.totalAmount < 0) {
      logger(Logging::DEBUGGING) << "tx " << id << " has negative amount";
      continue;
    }

    std::vector<uint8_t> extraVector(tx.extra.begin(), tx.extra.end());

    Crypto::Hash paymentId;
    if (!CryptoNote::getPaymentIdFromTxExtra(extraVector, paymentId)) {
      logger(Logging::DEBUGGING) << "tx " << id << " has no payment id";
      continue;
    }

    logger(Logging::DEBUGGING) << "transaction " << id << " has been inserted with payment id " << paymentId;
    insertTransaction(id, paymentId, tx.blockHeight != CryptoNote::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT);
  }
}

std::error_code WalletService::sendTransaction(const SendTransactionRequest& req, SendTransactionResponse& resp) {
  assert(wallet);
  logger(Logging::DEBUGGING) << "Send transaction request came";

  try {
    std::vector<CryptoNote::WalletTransfer> transfers;
    makeTransfers(req.destinations, transfers);

    std::string extra;
    if (!req.paymentId.empty()) {
      addPaymentIdToExtra(req.paymentId, extra);
    }

    size_t txId = wallet->transfer(transfers, req.fee, req.mixin, extra, req.unlockTime);
    if (txId == CryptoNote::WALLET_INVALID_TRANSACTION_ID) {
      logger(Logging::WARNING) << "Unable to send transaction";
      throw std::runtime_error("Error occured while sending transaction");
    }

    resp.transactionId = txId;
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Error while sending transaction: " << x.what();
    return x.code();
  }

  return std::error_code();
}

void WalletService::makeTransfers(const std::vector<PaymentService::TransferDestination>& destinations, std::vector<CryptoNote::WalletTransfer>& transfers) {
  transfers.reserve(destinations.size());

  for (auto dest: destinations) {
    transfers.push_back( { dest.address, static_cast<int64_t>(dest.amount) } );
  }
}

std::error_code WalletService::getAddress(size_t index, std::string& address) {
  logger(Logging::DEBUGGING) << "Get address request came";

  try {
    address = wallet->getAddress(index);
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Error while getting address: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getAddressCount(std::size_t& count) {
  logger(Logging::DEBUGGING) << "Get address count request came";
  count = wallet->getAddressCount();
  return std::error_code();
}

std::error_code WalletService::createAddress(std::string& address) {
  logger(Logging::DEBUGGING) << "Create address request came";

  try {
    address = wallet->createAddress();
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Error while creating address: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::deleteAddress(const std::string& address) {
  logger(Logging::DEBUGGING) << "Delete address request came";

  try {
    wallet->deleteAddress(address);
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Error while deleting address: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getActualBalance(const std::string& address, uint64_t& actualBalance) {
  logger(Logging::DEBUGGING) << "Get actual balance for address: " << address << " request came";

  try {
    actualBalance = wallet->getActualBalance(address);
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get actual balance: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getPendingBalance(const std::string& address, uint64_t& pendingBalance) {
  logger(Logging::DEBUGGING) << "Get pending balance for address: " << address <<" request came";

  try {
    pendingBalance = wallet->getPendingBalance(address);
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get pending balance: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getActualBalance(uint64_t& actualBalance) {
  logger(Logging::DEBUGGING) << "Get actual balance request came";

  try {
    actualBalance = wallet->getActualBalance();
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get actual balance: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getPendingBalance(uint64_t& pendingBalance) {
  logger(Logging::DEBUGGING) << "Get pending balance request came";

  try {
    pendingBalance = wallet->getPendingBalance();
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
  logger(Logging::DEBUGGING) << "Get transfers count request came";
  trCount = static_cast<uint64_t>(transfersIndices.back());
  return std::error_code();
}

std::error_code WalletService::getTransactionByTransferId(size_t transferId, size_t& transactionId) {
  logger(Logging::DEBUGGING) << "getTransactionByTransferId request came";

  if (transferId >= transfersIndices.back()) {
    logger(Logging::WARNING) << "Transfer ID:" << transferId<<" is out of domain";
    return std::make_error_code(std::errc::argument_out_of_domain);
  }

  auto nextTxId = std::upper_bound(transfersIndices.begin(), transfersIndices.end(), transferId);
  transactionId = (nextTxId - transfersIndices.begin()) - 1;

  return std::error_code();
}

void WalletService::fillTransactionRpcInfo(size_t txId, const CryptoNote::WalletTransaction& tx, TransactionRpcInfo& rpcInfo) {
  rpcInfo.firstTransferId = transfersIndices[txId];
  rpcInfo.transferCount = wallet->getTransactionTransferCount(txId);
  rpcInfo.totalAmount = tx.totalAmount;
  rpcInfo.fee = tx.fee;
  rpcInfo.blockHeight = tx.blockHeight;
  rpcInfo.timestamp = tx.timestamp;
  rpcInfo.extra = Common::toHex(tx.extra.data(), tx.extra.size());
  rpcInfo.hash = Common::podToHex(tx.hash);
  for (size_t transferId = 0; transferId < rpcInfo.transferCount; ++transferId) {
    auto transfer = wallet->getTransactionTransfer(txId, transferId);
    TransferRpcInfo rpcTransfer{ transfer.address, transfer.amount };
    rpcInfo.transfers.push_back(rpcTransfer);
  }
}

std::error_code WalletService::getTransaction(size_t txId, bool& found, TransactionRpcInfo& rpcInfo) {
  logger(Logging::DEBUGGING) << "getTransaction request came";

  found = false;

  try {
    if (txId + 1 >= transfersIndices.size()) {
      logger(Logging::WARNING) << "Unable to get transaction " << txId << ": argument out of domain.";
      return std::make_error_code(std::errc::argument_out_of_domain);
    }

    auto tx = wallet->getTransaction(txId);

    fillTransactionRpcInfo(txId, tx, rpcInfo);

    found = true;
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get transaction: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::listTransactions(size_t startingTxId, uint32_t maxTxCount, std::vector<TransactionRpcInfo>& txsRpcInfo) {
  logger(Logging::DEBUGGING) << "listTransactions request came";

  if (maxTxCount == 0) {
    txsRpcInfo.clear();
    return std::error_code();
  }

  try {
    size_t endTxId;
    if (startingTxId > std::numeric_limits<size_t>::max() - static_cast<size_t>(maxTxCount)) {
      endTxId = static_cast<size_t>(wallet->getTransactionCount());
    } else {
      endTxId = startingTxId + static_cast<size_t>(maxTxCount);
      endTxId = std::min(endTxId, static_cast<size_t>(wallet->getTransactionCount()));
    }

    txsRpcInfo.resize(endTxId - startingTxId);

    for (auto txId = startingTxId; txId < endTxId; ++txId) {
      assert(txId < wallet->getTransactionCount());
      auto tx = wallet->getTransaction(txId);
      fillTransactionRpcInfo(txId, tx, txsRpcInfo[txId - startingTxId]);
    }
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to list transaction: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getTransfer(size_t globalTransferId, bool& found, TransferRpcInfo& rpcInfo) {
  logger(Logging::DEBUGGING) << "getTransfer request came";
  found = false;
  try {
    size_t txId = (std::upper_bound(transfersIndices.begin(), transfersIndices.end(), globalTransferId) - transfersIndices.begin()) - 1;
    size_t fakeTxId = transfersIndices.size() - 1;

    if (txId == fakeTxId) {
      return std::error_code();
    }

    auto transferId = globalTransferId - transfersIndices[txId];
    auto transfer = wallet->getTransactionTransfer(txId, transferId);

    rpcInfo.address = transfer.address;
    rpcInfo.amount = transfer.amount;
    found = true;
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get transfer: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getIncomingPayments(const std::vector<std::string>& payments, IncomingPayments& result) {
  logger(Logging::DEBUGGING) << "getIncomingPayments request came";

  try {
    for (const std::string& payment: payments) {

      if (!checkPaymentId(payment)) {
        return make_error_code(std::errc::argument_out_of_domain);
      }

      std::string paymentString = payment;
      std::transform(paymentString.begin(), paymentString.end(), paymentString.begin(), ::tolower);
      auto pair = paymentIdIndex.equal_range(paymentString);

      for (auto it = pair.first; it != pair.second; ++it) {
        auto tx = wallet->getTransaction(it->transactionId);

        std::string hashString = Common::podToHex(tx.hash);

        PaymentDetails details;
        details.txHash = std::move(hashString);
        details.amount = static_cast<uint64_t>(tx.totalAmount);
        details.blockHeight = tx.blockHeight;
        details.unlockTime = tx.unlockTime;

        result[it->paymentId].push_back(std::move(details));
      }
    }
  } catch (std::system_error& x) {
    logger(Logging::WARNING) << "Unable to get payments: " << x.what();
    return x.code();
  }

  return std::error_code();
}

void WalletService::refresh() {
  try {
    for (;;) {
      auto event = wallet->getEvent();
      if (event.type == CryptoNote::TRANSACTION_CREATED || event.type == CryptoNote::TRANSACTION_UPDATED) {
        size_t transactionId;
        if (event.type == CryptoNote::TRANSACTION_CREATED) {
          transactionId = event.transactionCreated.transactionIndex;
          transfersIndices.push_back(transfersIndices[transactionId] + wallet->getTransactionTransferCount(transactionId));
        } else {
          transactionId = event.transactionUpdated.transactionIndex;
        }

        auto tx = wallet->getTransaction(transactionId);
        logger(Logging::DEBUGGING) << "Transaction updated " << transactionId << " extra size: " << tx.extra.size();
        if (tx.totalAmount < 0) {
          continue;
        }

        std::vector<uint8_t> extraVector(tx.extra.begin(), tx.extra.end());
        Crypto::Hash paymentId;
        if (!CryptoNote::getPaymentIdFromTxExtra(extraVector, paymentId)) {
          logger(Logging::DEBUGGING) << "transaction " << transactionId << " has no payment id";
          continue;
        }

        insertTransaction(transactionId, paymentId, tx.blockHeight != CryptoNote::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT);
        logger(Logging::DEBUGGING) << "transaction " << transactionId << " has been added to payments cache";
      }
    }
  } catch (std::system_error& e) {
    logger(Logging::TRACE) << "refresh is stopped: " << e.what();
  } catch (std::exception& e) {
    logger(Logging::WARNING) << "exception thrown in refresh(): " << e.what();
  }
}

void WalletService::insertTransaction(size_t id, const Crypto::Hash& paymentIdBin, bool confirmed) {
  paymentsCache.insert(PaymentItem{ Common::podToHex(paymentIdBin), id, confirmed});
}

} //namespace PaymentService
