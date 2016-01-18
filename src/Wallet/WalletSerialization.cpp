// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletSerialization.h"

#include <string>
#include <sstream>
#include <type_traits>

#include "Common/MemoryInputStream.h"
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

#include "Serialization/BinaryOutputStreamSerializer.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/SerializationOverloads.h"

#include "Wallet/WalletErrors.h"
#include "WalletLegacy/KeysStorage.h"
#include "WalletLegacy/WalletLegacySerialization.h"

using namespace Common;
using namespace Crypto;

namespace {

//DO NOT CHANGE IT
struct WalletRecordDto {
  PublicKey spendPublicKey;
  SecretKey spendSecretKey;
  uint64_t pendingBalance = 0;
  uint64_t actualBalance = 0;
  uint64_t creationTimestamp = 0;
};

//DO NOT CHANGE IT
struct ObsoleteSpentOutputDto {
  uint64_t amount;
  Hash transactionHash;
  uint32_t outputInTransaction;
  uint64_t walletIndex;
  Crypto::Hash spendingTransactionHash;
};

//DO NOT CHANGE IT
struct ObsoleteChangeDto {
  Hash txHash;
  uint64_t amount;
};

//DO NOT CHANGE IT
struct UnlockTransactionJobDto {
  uint32_t blockHeight;
  Hash transactionHash;
  uint64_t walletIndex;
};

//DO NOT CHANGE IT
struct WalletTransactionDto {
  WalletTransactionDto() {}

  WalletTransactionDto(const CryptoNote::WalletTransaction& wallet) {
    state = wallet.state;
    timestamp = wallet.timestamp;
    blockHeight = wallet.blockHeight;
    hash = wallet.hash;
    totalAmount = wallet.totalAmount;
    fee = wallet.fee;
    creationTime = wallet.creationTime;
    unlockTime = wallet.unlockTime;
    extra = wallet.extra;
  }

  CryptoNote::WalletTransactionState state;
  uint64_t timestamp;
  uint32_t blockHeight;
  Hash hash;
  int64_t totalAmount;
  uint64_t fee;
  uint64_t creationTime;
  uint64_t unlockTime;
  std::string extra;
};

//DO NOT CHANGE IT
struct WalletTransferDto {
  WalletTransferDto(uint32_t version) : version(version) {}
  WalletTransferDto(const CryptoNote::WalletTransfer& tr, uint32_t version) : WalletTransferDto(version) {
    address = tr.address;
    amount = tr.amount;
    type = static_cast<uint8_t>(tr.type);
  }

  std::string address;
  uint64_t amount;
  uint8_t type;

  uint32_t version;
};

void serialize(WalletRecordDto& value, CryptoNote::ISerializer& serializer) {
  serializer(value.spendPublicKey, "spend_public_key");
  serializer(value.spendSecretKey, "spend_secret_key");
  serializer(value.pendingBalance, "pending_balance");
  serializer(value.actualBalance, "actual_balance");
  serializer(value.creationTimestamp, "creation_timestamp");
}

void serialize(ObsoleteSpentOutputDto& value, CryptoNote::ISerializer& serializer) {
  serializer(value.amount, "amount");
  serializer(value.transactionHash, "transaction_hash");
  serializer(value.outputInTransaction, "output_in_transaction");
  serializer(value.walletIndex, "wallet_index");
  serializer(value.spendingTransactionHash, "spending_transaction_hash");
}

void serialize(ObsoleteChangeDto& value, CryptoNote::ISerializer& serializer) {
  serializer(value.txHash, "transaction_hash");
  serializer(value.amount, "amount");
}

void serialize(UnlockTransactionJobDto& value, CryptoNote::ISerializer& serializer) {
  serializer(value.blockHeight, "block_height");
  serializer(value.transactionHash, "transaction_hash");
  serializer(value.walletIndex, "wallet_index");
}

void serialize(WalletTransactionDto& value, CryptoNote::ISerializer& serializer) {
  typedef std::underlying_type<CryptoNote::WalletTransactionState>::type StateType;

  StateType state = static_cast<StateType>(value.state);
  serializer(state, "state");
  value.state = static_cast<CryptoNote::WalletTransactionState>(state);

  serializer(value.timestamp, "timestamp");
  CryptoNote::serializeBlockHeight(serializer, value.blockHeight, "block_height");
  serializer(value.hash, "hash");
  serializer(value.totalAmount, "total_amount");
  serializer(value.fee, "fee");
  serializer(value.creationTime, "creation_time");
  serializer(value.unlockTime, "unlock_time");
  serializer(value.extra, "extra");
}

void serialize(WalletTransferDto& value, CryptoNote::ISerializer& serializer) {
  serializer(value.address, "address");
  serializer(value.amount, "amount");

  if (value.version > 2) {
    serializer(value.type, "type");
  }
}

template <typename Object>
std::string serialize(Object& obj, const std::string& name) {
  std::stringstream stream;
  StdOutputStream output(stream);
  CryptoNote::BinaryOutputStreamSerializer s(output);

  s(obj, Common::StringView(name));

  stream.flush();
  return stream.str();
}

std::string encrypt(const std::string& plain, CryptoNote::CryptoContext& cryptoContext) {
  std::string cipher;
  cipher.resize(plain.size());

  Crypto::chacha8(plain.data(), plain.size(), cryptoContext.key, cryptoContext.iv, &cipher[0]);

  return cipher;
}

void addToStream(const std::string& cipher, const std::string& name, Common::IOutputStream& destination) {
  CryptoNote::BinaryOutputStreamSerializer s(destination);
  s(const_cast<std::string& >(cipher), name);
}

template<typename Object>
void serializeEncrypted(Object& obj, const std::string& name, CryptoNote::CryptoContext& cryptoContext, Common::IOutputStream& destination) {
  std::string plain = serialize(obj, name);
  std::string cipher = encrypt(plain, cryptoContext);

  addToStream(cipher, name, destination);
}

std::string readCipher(Common::IInputStream& source, const std::string& name) {
  std::string cipher;
  CryptoNote::BinaryInputStreamSerializer s(source);
  s(cipher, name);

  return cipher;
}

std::string decrypt(const std::string& cipher, CryptoNote::CryptoContext& cryptoContext) {
  std::string plain;
  plain.resize(cipher.size());

  Crypto::chacha8(cipher.data(), cipher.size(), cryptoContext.key, cryptoContext.iv, &plain[0]);
  return plain;
}

template<typename Object>
void deserialize(Object& obj, const std::string& name, const std::string& plain) {
  MemoryInputStream stream(plain.data(), plain.size());
  CryptoNote::BinaryInputStreamSerializer s(stream);
  s(obj, Common::StringView(name));
}

template<typename Object>
void deserializeEncrypted(Object& obj, const std::string& name, CryptoNote::CryptoContext& cryptoContext, Common::IInputStream& source) {
  std::string cipher = readCipher(source, name);
  std::string plain = decrypt(cipher, cryptoContext);

  deserialize(obj, name, plain);
}

bool verifyKeys(const SecretKey& sec, const PublicKey& expected_pub) {
  PublicKey pub;
  bool r = Crypto::secret_key_to_public_key(sec, pub);

  return r && expected_pub == pub;
}

void throwIfKeysMissmatch(const SecretKey& sec, const PublicKey& expected_pub) {
  if (!verifyKeys(sec, expected_pub))
    throw std::system_error(make_error_code(CryptoNote::error::WRONG_PASSWORD));
}

CryptoNote::WalletTransaction convert(const CryptoNote::WalletLegacyTransaction& tx) {
  CryptoNote::WalletTransaction mtx;

  mtx.state = CryptoNote::WalletTransactionState::SUCCEEDED;
  mtx.timestamp = tx.timestamp;
  mtx.blockHeight = tx.blockHeight;
  mtx.hash = tx.hash;
  mtx.totalAmount = tx.totalAmount;
  mtx.fee = tx.fee;
  mtx.creationTime = tx.sentTime;
  mtx.unlockTime = tx.unlockTime;
  mtx.extra = tx.extra;
  mtx.isBase = tx.isCoinbase;

  return mtx;
}

CryptoNote::WalletTransfer convert(const CryptoNote::WalletLegacyTransfer& tr) {
  CryptoNote::WalletTransfer mtr;

  mtr.address = tr.address;
  mtr.amount = tr.amount;

  return mtr;
}

}

namespace CryptoNote {

const uint32_t WalletSerializer::SERIALIZATION_VERSION = 5;

void CryptoContext::incIv() {
  uint64_t * i = reinterpret_cast<uint64_t *>(&iv.data[0]);
  (*i)++;
}

WalletSerializer::WalletSerializer(
  ITransfersObserver& transfersObserver,
  PublicKey& viewPublicKey,
  SecretKey& viewSecretKey,
  uint64_t& actualBalance,
  uint64_t& pendingBalance,
  WalletsContainer& walletsContainer,
  TransfersSyncronizer& synchronizer,
  UnlockTransactionJobs& unlockTransactions,
  WalletTransactions& transactions,
  WalletTransfers& transfers,
  uint32_t transactionSoftLockTime,
  UncommitedTransactions& uncommitedTransactions
) :
  m_transfersObserver(transfersObserver),
  m_viewPublicKey(viewPublicKey),
  m_viewSecretKey(viewSecretKey),
  m_actualBalance(actualBalance),
  m_pendingBalance(pendingBalance),
  m_walletsContainer(walletsContainer),
  m_synchronizer(synchronizer),
  m_unlockTransactions(unlockTransactions),
  m_transactions(transactions),
  m_transfers(transfers),
  m_transactionSoftLockTime(transactionSoftLockTime),
  uncommitedTransactions(uncommitedTransactions)
{ }

void WalletSerializer::save(const std::string& password, Common::IOutputStream& destination, bool saveDetails, bool saveCache) {
  CryptoContext cryptoContext = generateCryptoContext(password);

  CryptoNote::BinaryOutputStreamSerializer s(destination);
  s.beginObject("wallet");

  saveVersion(destination);
  saveIv(destination, cryptoContext.iv);

  saveKeys(destination, cryptoContext);
  saveWallets(destination, saveCache, cryptoContext);
  saveFlags(saveDetails, saveCache, destination, cryptoContext);

  if (saveDetails) {
    saveTransactions(destination, cryptoContext);
    saveTransfers(destination, cryptoContext);
  }

  if (saveCache) {
    saveBalances(destination, saveCache, cryptoContext);
    saveTransfersSynchronizer(destination, cryptoContext);
    saveUnlockTransactionsJobs(destination, cryptoContext);
    saveUncommitedTransactions(destination, cryptoContext);
  }

  s.endObject();
}

CryptoContext WalletSerializer::generateCryptoContext(const std::string& password) {
  CryptoContext context;

  Crypto::cn_context c;
  Crypto::generate_chacha8_key(c, password, context.key);

  context.iv = Crypto::rand<Crypto::chacha8_iv>();

  return context;
}

void WalletSerializer::saveVersion(Common::IOutputStream& destination) {
  uint32_t version = SERIALIZATION_VERSION;

  BinaryOutputStreamSerializer s(destination);
  s(version, "version");
}

void WalletSerializer::saveIv(Common::IOutputStream& destination, Crypto::chacha8_iv& iv) {
  BinaryOutputStreamSerializer s(destination);
  s.binary(reinterpret_cast<void *>(&iv.data), sizeof(iv.data), "chacha_iv");
}

void WalletSerializer::saveKeys(Common::IOutputStream& destination, CryptoContext& cryptoContext) {
  savePublicKey(destination, cryptoContext);
  saveSecretKey(destination, cryptoContext);
}

void WalletSerializer::savePublicKey(Common::IOutputStream& destination, CryptoContext& cryptoContext) {
  serializeEncrypted(m_viewPublicKey, "public_key", cryptoContext, destination);
  cryptoContext.incIv();
}

void WalletSerializer::saveSecretKey(Common::IOutputStream& destination, CryptoContext& cryptoContext) {
  serializeEncrypted(m_viewSecretKey, "secret_key", cryptoContext, destination);
  cryptoContext.incIv();
}

void WalletSerializer::saveFlags(bool saveDetails, bool saveCache, Common::IOutputStream& destination, CryptoContext& cryptoContext) {
  serializeEncrypted(saveDetails, "details", cryptoContext, destination);
  cryptoContext.incIv();

  serializeEncrypted(saveCache, "cache", cryptoContext, destination);
  cryptoContext.incIv();
}

void WalletSerializer::saveWallets(Common::IOutputStream& destination, bool saveCache, CryptoContext& cryptoContext) {
  auto& index = m_walletsContainer.get<RandomAccessIndex>();

  uint64_t count = index.size();
  serializeEncrypted(count, "wallets_count", cryptoContext, destination);
  cryptoContext.incIv();

  for (const auto& w: index) {
    WalletRecordDto dto;
    dto.spendPublicKey = w.spendPublicKey;
    dto.spendSecretKey = w.spendSecretKey;
    dto.pendingBalance = saveCache ? w.pendingBalance : 0;
    dto.actualBalance = saveCache ? w.actualBalance : 0;
    dto.creationTimestamp = static_cast<uint64_t>(w.creationTimestamp);

    serializeEncrypted(dto, "", cryptoContext, destination);
    cryptoContext.incIv();
  }
}

void WalletSerializer::saveBalances(Common::IOutputStream& destination, bool saveCache, CryptoContext& cryptoContext) {
  uint64_t actual = saveCache ? m_actualBalance : 0;
  uint64_t pending = saveCache ? m_pendingBalance : 0;

  serializeEncrypted(actual, "actual_balance", cryptoContext, destination);
  cryptoContext.incIv();

  serializeEncrypted(pending, "pending_balance", cryptoContext, destination);
  cryptoContext.incIv();
}

void WalletSerializer::saveTransfersSynchronizer(Common::IOutputStream& destination, CryptoContext& cryptoContext) {
  std::stringstream stream;
  m_synchronizer.save(stream);
  stream.flush();

  std::string plain = stream.str();
  serializeEncrypted(plain, "transfers_synchronizer", cryptoContext, destination);
  cryptoContext.incIv();
}

void WalletSerializer::saveUnlockTransactionsJobs(Common::IOutputStream& destination, CryptoContext& cryptoContext) {
  auto& index = m_unlockTransactions.get<TransactionHashIndex>();
  auto& wallets = m_walletsContainer.get<TransfersContainerIndex>();

  uint64_t jobsCount = index.size();
  serializeEncrypted(jobsCount, "unlock_transactions_jobs_count", cryptoContext, destination);
  cryptoContext.incIv();

  for (const auto& j: index) {
    auto containerIt = wallets.find(j.container);
    assert(containerIt != wallets.end());

    auto rndIt = m_walletsContainer.project<RandomAccessIndex>(containerIt);
    assert(rndIt != m_walletsContainer.get<RandomAccessIndex>().end());

    uint64_t walletIndex = std::distance(m_walletsContainer.get<RandomAccessIndex>().begin(), rndIt);

    UnlockTransactionJobDto dto;
    dto.blockHeight = j.blockHeight;
    dto.transactionHash = j.transactionHash;
    dto.walletIndex = walletIndex;

    serializeEncrypted(dto, "", cryptoContext, destination);
    cryptoContext.incIv();
  }
}

void WalletSerializer::saveUncommitedTransactions(Common::IOutputStream& destination, CryptoContext& cryptoContext) {
  serializeEncrypted(uncommitedTransactions, "uncommited_transactions", cryptoContext, destination);
}

void WalletSerializer::saveTransactions(Common::IOutputStream& destination, CryptoContext& cryptoContext) {
  uint64_t count = m_transactions.size();
  serializeEncrypted(count, "transactions_count", cryptoContext, destination);
  cryptoContext.incIv();

  for (const auto& tx: m_transactions) {
    WalletTransactionDto dto(tx);
    serializeEncrypted(dto, "", cryptoContext, destination);
    cryptoContext.incIv();
  }
}

void WalletSerializer::saveTransfers(Common::IOutputStream& destination, CryptoContext& cryptoContext) {
  uint64_t count = m_transfers.size();
  serializeEncrypted(count, "transfers_count", cryptoContext, destination);
  cryptoContext.incIv();

  for (const auto& kv: m_transfers) {
    uint64_t txId = kv.first;

    WalletTransferDto tr(kv.second, SERIALIZATION_VERSION);

    serializeEncrypted(txId, "transaction_id", cryptoContext, destination);
    cryptoContext.incIv();

    serializeEncrypted(tr, "transfer", cryptoContext, destination);
    cryptoContext.incIv();
  }
}

void WalletSerializer::load(const std::string& password, Common::IInputStream& source) {
  CryptoNote::BinaryInputStreamSerializer s(source);
  s.beginObject("wallet");

  uint32_t version = loadVersion(source);

  if (version > SERIALIZATION_VERSION) {
    throw std::system_error(make_error_code(error::WRONG_VERSION));
  } else if (version != 1) {
    loadWallet(source, password, version);
  } else {
    loadWalletV1(source, password);
  }

  s.endObject();
}

void WalletSerializer::loadWallet(Common::IInputStream& source, const std::string& password, uint32_t version) {
  CryptoNote::CryptoContext cryptoContext;

  bool details = false;
  bool cache = false;

  loadIv(source, cryptoContext.iv);
  generateKey(password, cryptoContext.key);

  loadKeys(source, cryptoContext);
  checkKeys();

  loadWallets(source, cryptoContext);
  subscribeWallets();

  loadFlags(details, cache, source, cryptoContext);

  if (details) {
    loadTransactions(source, cryptoContext);
    loadTransfers(source, cryptoContext, version);
  }

  if (version < 5) {
    updateTransfersSign();
    cache = false;
  }

  if (cache) {
    loadBalances(source, cryptoContext);
    loadTransfersSynchronizer(source, cryptoContext);
    if (version < 5) {
      loadObsoleteSpentOutputs(source, cryptoContext);
    }

    loadUnlockTransactionsJobs(source, cryptoContext);

    if (version < 5) {
      loadObsoleteChange(source, cryptoContext);
    }

    if (version > 3) {
      loadUncommitedTransactions(source, cryptoContext);

      if (version >= 5) {
        initTransactionPool();
      }
    }
  } else {
    resetCachedBalance();
  }

  if (details && cache) {
    updateTransactionsBaseStatus();
  }
}

void WalletSerializer::loadWalletV1(Common::IInputStream& source, const std::string& password) {
  CryptoNote::CryptoContext cryptoContext;

  CryptoNote::BinaryInputStreamSerializer encrypted(source);

  encrypted(cryptoContext.iv, "iv");
  generateKey(password, cryptoContext.key);

  std::string cipher;
  encrypted(cipher, "data");

  std::string plain = decrypt(cipher, cryptoContext);

  MemoryInputStream decryptedStream(plain.data(), plain.size());
  CryptoNote::BinaryInputStreamSerializer serializer(decryptedStream);

  loadWalletV1Keys(serializer);
  checkKeys();

  subscribeWallets();

  bool detailsSaved;
  serializer(detailsSaved, "has_details");

  if (detailsSaved) {
    loadWalletV1Details(serializer);
  }
}

void WalletSerializer::loadWalletV1Keys(CryptoNote::BinaryInputStreamSerializer& serializer) {
  CryptoNote::KeysStorage keys;
  keys.serialize(serializer, "keys");

  m_viewPublicKey = keys.viewPublicKey;
  m_viewSecretKey = keys.viewSecretKey;

  WalletRecord wallet;
  wallet.spendPublicKey = keys.spendPublicKey;
  wallet.spendSecretKey = keys.spendSecretKey;
  wallet.actualBalance = 0;
  wallet.pendingBalance = 0;
  wallet.creationTimestamp = static_cast<time_t>(keys.creationTimestamp);

  m_walletsContainer.get<RandomAccessIndex>().push_back(wallet);
}

void WalletSerializer::loadWalletV1Details(CryptoNote::BinaryInputStreamSerializer& serializer) {
  std::vector<WalletLegacyTransaction> txs;
  std::vector<WalletLegacyTransfer> trs;

  serializer(txs, "transactions");
  serializer(trs, "transfers");

  addWalletV1Details(txs, trs);
}

uint32_t WalletSerializer::loadVersion(Common::IInputStream& source) {
  CryptoNote::BinaryInputStreamSerializer s(source);

  uint32_t version = std::numeric_limits<uint32_t>::max();
  s(version, "version");

  return version;
}

void WalletSerializer::loadIv(Common::IInputStream& source, Crypto::chacha8_iv& iv) {
  CryptoNote::BinaryInputStreamSerializer s(source);

  s.binary(static_cast<void *>(&iv.data), sizeof(iv.data), "chacha_iv");
}

void WalletSerializer::generateKey(const std::string& password, Crypto::chacha8_key& key) {
  Crypto::cn_context context;
  Crypto::generate_chacha8_key(context, password, key);
}

void WalletSerializer::loadKeys(Common::IInputStream& source, CryptoContext& cryptoContext) {
  loadPublicKey(source, cryptoContext);
  loadSecretKey(source, cryptoContext);
}

void WalletSerializer::loadPublicKey(Common::IInputStream& source, CryptoContext& cryptoContext) {
  deserializeEncrypted(m_viewPublicKey, "public_key", cryptoContext, source);
  cryptoContext.incIv();
}

void WalletSerializer::loadSecretKey(Common::IInputStream& source, CryptoContext& cryptoContext) {
  deserializeEncrypted(m_viewSecretKey, "secret_key", cryptoContext, source);
  cryptoContext.incIv();
}

void WalletSerializer::checkKeys() {
  throwIfKeysMissmatch(m_viewSecretKey, m_viewPublicKey);
}

void WalletSerializer::loadFlags(bool& details, bool& cache, Common::IInputStream& source, CryptoContext& cryptoContext) {
  deserializeEncrypted(details, "details", cryptoContext, source);
  cryptoContext.incIv();

  deserializeEncrypted(cache, "cache", cryptoContext, source);
  cryptoContext.incIv();
}

void WalletSerializer::loadWallets(Common::IInputStream& source, CryptoContext& cryptoContext) {
  auto& index = m_walletsContainer.get<RandomAccessIndex>();

  uint64_t count = 0;
  deserializeEncrypted(count, "wallets_count", cryptoContext, source);
  cryptoContext.incIv();

  bool isTrackingMode;

  for (uint64_t i = 0; i < count; ++i) {
    WalletRecordDto dto;
    deserializeEncrypted(dto, "", cryptoContext, source);
    cryptoContext.incIv();

    if (i == 0) {
      isTrackingMode = dto.spendSecretKey == NULL_SECRET_KEY;
    } else if ((isTrackingMode && dto.spendSecretKey != NULL_SECRET_KEY) || (!isTrackingMode && dto.spendSecretKey == NULL_SECRET_KEY)) {
      throw std::system_error(make_error_code(error::BAD_ADDRESS), "All addresses must be whether tracking or not");
    }

    if (dto.spendSecretKey != NULL_SECRET_KEY) {
      Crypto::PublicKey restoredPublicKey;
      bool r = Crypto::secret_key_to_public_key(dto.spendSecretKey, restoredPublicKey);

      if (!r || dto.spendPublicKey != restoredPublicKey) {
        throw std::system_error(make_error_code(error::WRONG_PASSWORD), "Restored spend public key doesn't correspond to secret key");
      }
    } else {
      if (!Crypto::check_key(dto.spendPublicKey)) {
        throw std::system_error(make_error_code(error::WRONG_PASSWORD), "Public spend key is incorrect");
      }
    }

    WalletRecord wallet;
    wallet.spendPublicKey = dto.spendPublicKey;
    wallet.spendSecretKey = dto.spendSecretKey;
    wallet.actualBalance = dto.actualBalance;
    wallet.pendingBalance = dto.pendingBalance;
    wallet.creationTimestamp = static_cast<time_t>(dto.creationTimestamp);
    wallet.container = reinterpret_cast<CryptoNote::ITransfersContainer*>(i); //dirty hack. container field must be unique

    index.push_back(wallet);
  }
}

void WalletSerializer::subscribeWallets() {
  auto& index = m_walletsContainer.get<RandomAccessIndex>();

  for (auto it = index.begin(); it != index.end(); ++it) {
    const auto& wallet = *it;

    AccountSubscription sub;
    sub.keys.address.viewPublicKey = m_viewPublicKey;
    sub.keys.address.spendPublicKey = wallet.spendPublicKey;
    sub.keys.viewSecretKey = m_viewSecretKey;
    sub.keys.spendSecretKey = wallet.spendSecretKey;
    sub.transactionSpendableAge = m_transactionSoftLockTime;
    sub.syncStart.height = 0;
    sub.syncStart.timestamp = std::max(static_cast<uint64_t>(wallet.creationTimestamp), ACCOUNT_CREATE_TIME_ACCURACY) - ACCOUNT_CREATE_TIME_ACCURACY;

    auto& subscription = m_synchronizer.addSubscription(sub);
    bool r = index.modify(it, [&subscription] (WalletRecord& rec) { rec.container = &subscription.getContainer(); });
    assert(r);

    subscription.addObserver(&m_transfersObserver);
  }
}

void WalletSerializer::loadBalances(Common::IInputStream& source, CryptoContext& cryptoContext) {
  deserializeEncrypted(m_actualBalance, "actual_balance", cryptoContext, source);
  cryptoContext.incIv();

  deserializeEncrypted(m_pendingBalance, "pending_balance", cryptoContext, source);
  cryptoContext.incIv();
}

void WalletSerializer::loadTransfersSynchronizer(Common::IInputStream& source, CryptoContext& cryptoContext) {
  std::string deciphered;
  deserializeEncrypted(deciphered, "transfers_synchronizer", cryptoContext, source);
  cryptoContext.incIv();

  std::stringstream stream(deciphered);
  deciphered.clear();

  m_synchronizer.load(stream);
}

void WalletSerializer::loadObsoleteSpentOutputs(Common::IInputStream& source, CryptoContext& cryptoContext) {
  uint64_t count = 0;
  deserializeEncrypted(count, "spent_outputs_count", cryptoContext, source);
  cryptoContext.incIv();

  for (uint64_t i = 0; i < count; ++i) {
    ObsoleteSpentOutputDto dto;
    deserializeEncrypted(dto, "", cryptoContext, source);
    cryptoContext.incIv();
  }
}

void WalletSerializer::loadUnlockTransactionsJobs(Common::IInputStream& source, CryptoContext& cryptoContext) {
  auto& index = m_unlockTransactions.get<TransactionHashIndex>();
  auto& walletsIndex = m_walletsContainer.get<RandomAccessIndex>();
  const uint64_t walletsSize = walletsIndex.size();

  uint64_t jobsCount = 0;
  deserializeEncrypted(jobsCount, "unlock_transactions_jobs_count", cryptoContext, source);
  cryptoContext.incIv();

  for (uint64_t i = 0; i < jobsCount; ++i) {
    UnlockTransactionJobDto dto;
    deserializeEncrypted(dto, "", cryptoContext, source);
    cryptoContext.incIv();

    assert(dto.walletIndex < walletsSize);

    UnlockTransactionJob job;
    job.blockHeight = dto.blockHeight;
    job.transactionHash = dto.transactionHash;
    job.container = walletsIndex[dto.walletIndex].container;

    index.insert(std::move(job));
  }
}

void WalletSerializer::loadObsoleteChange(Common::IInputStream& source, CryptoContext& cryptoContext) {
  uint64_t count = 0;
  deserializeEncrypted(count, "changes_count", cryptoContext, source);
  cryptoContext.incIv();

  for (uint64_t i = 0; i < count; i++) {
    ObsoleteChangeDto dto;
    deserializeEncrypted(dto, "", cryptoContext, source);
    cryptoContext.incIv();
  }
}

void WalletSerializer::loadUncommitedTransactions(Common::IInputStream& source, CryptoContext& cryptoContext) {
  deserializeEncrypted(uncommitedTransactions, "uncommited_transactions", cryptoContext, source);
}

void WalletSerializer::initTransactionPool() {
  std::unordered_set<Crypto::Hash> uncommitedTransactionsSet;
  std::transform(uncommitedTransactions.begin(), uncommitedTransactions.end(), std::inserter(uncommitedTransactionsSet, uncommitedTransactionsSet.end()),
    [](const UncommitedTransactions::value_type& pair) {
      return getObjectHash(pair.second);
    });
  m_synchronizer.initTransactionPool(uncommitedTransactionsSet);
}

void WalletSerializer::resetCachedBalance() {
  for (auto it = m_walletsContainer.begin(); it != m_walletsContainer.end(); ++it) {
    m_walletsContainer.modify(it, [](WalletRecord& wallet) {
      wallet.actualBalance = 0;
      wallet.pendingBalance = 0;
    });
  }
}

// can't do it in loadTransactions, TransfersContainer is not yet loaded
void WalletSerializer::updateTransactionsBaseStatus() {
  auto& transactions = m_transactions.get<RandomAccessIndex>();
  auto begin = std::begin(transactions);
  auto end = std::end(transactions);
  for (; begin != end; ++begin) {
    transactions.modify(begin, [this](WalletTransaction& tx) {
      auto& wallets = m_walletsContainer.get<RandomAccessIndex>();
      TransactionInformation txInfo;
      auto it = std::find_if(std::begin(wallets), std::end(wallets), [&](const WalletRecord& rec) {
        assert(rec.container != nullptr);
        return rec.container->getTransactionInformation(tx.hash, txInfo);
      });

      tx.isBase = it != std::end(wallets) && txInfo.totalAmountIn == 0;
    });
  }
}

void WalletSerializer::updateTransfersSign() {
  auto it = m_transfers.begin();
  while (it != m_transfers.end()) {
    if (it->second.amount < 0) {
      it->second.amount = -it->second.amount;
      ++it;
    } else {
      it = m_transfers.erase(it);
    }
  }
}

void WalletSerializer::loadTransactions(Common::IInputStream& source, CryptoContext& cryptoContext) {
  uint64_t count = 0;
  deserializeEncrypted(count, "transactions_count", cryptoContext, source);
  cryptoContext.incIv();

  m_transactions.get<RandomAccessIndex>().reserve(count);

  for (uint64_t i = 0; i < count; ++i) {
    WalletTransactionDto dto;
    deserializeEncrypted(dto, "", cryptoContext, source);
    cryptoContext.incIv();

    WalletTransaction tx;
    tx.state = dto.state;
    tx.timestamp = dto.timestamp;
    tx.blockHeight = dto.blockHeight;
    tx.hash = dto.hash;
    tx.totalAmount = dto.totalAmount;
    tx.fee = dto.fee;
    tx.creationTime = dto.creationTime;
    tx.unlockTime = dto.unlockTime;
    tx.extra = dto.extra;
    tx.isBase = false;

    m_transactions.get<RandomAccessIndex>().push_back(std::move(tx));
  }
}

void WalletSerializer::loadTransfers(Common::IInputStream& source, CryptoContext& cryptoContext, uint32_t version) {
  uint64_t count = 0;
  deserializeEncrypted(count, "transfers_count", cryptoContext, source);
  cryptoContext.incIv();

  m_transfers.reserve(count);

  for (uint64_t i = 0; i < count; ++i) {
    uint64_t txId = 0;
    deserializeEncrypted(txId, "transaction_id", cryptoContext, source);
    cryptoContext.incIv();

    WalletTransferDto dto(version);
    deserializeEncrypted(dto, "transfer", cryptoContext, source);
    cryptoContext.incIv();

    WalletTransfer tr;
    tr.address = dto.address;
    tr.amount = dto.amount;

    if (version > 2) {
      tr.type = static_cast<WalletTransferType>(dto.type);
    } else {
      tr.type = WalletTransferType::USUAL;
    }

    m_transfers.push_back(std::make_pair(txId, tr));
  }
}

void WalletSerializer::addWalletV1Details(const std::vector<WalletLegacyTransaction>& txs, const std::vector<WalletLegacyTransfer>& trs) {
  size_t txId = 0;
  m_transfers.reserve(trs.size());

  for (const auto& tx: txs) {
    WalletTransaction mtx = convert(tx);
    m_transactions.get<RandomAccessIndex>().push_back(std::move(mtx));

    if (tx.firstTransferId != WALLET_LEGACY_INVALID_TRANSFER_ID && tx.transferCount != 0) {
      size_t firstTr = tx.firstTransferId;
      size_t lastTr = firstTr + tx.transferCount;

      if (lastTr > trs.size()) {
        throw std::system_error(make_error_code(error::INTERNAL_WALLET_ERROR));
      }

      for (; firstTr < lastTr; firstTr++) {
        WalletTransfer tr = convert(trs[firstTr]);
        m_transfers.push_back(std::make_pair(txId, tr));
      }
    }

    txId++;
  }
}

} //namespace CryptoNote
