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

#include "TransactionApiHelpers.h"
#include "CryptoNoteCore/TransactionApi.h"

using namespace CryptoNote;
using namespace Crypto;

namespace {

  const std::vector<AccountBase>& getMsigAccounts() {
    static std::vector<AccountBase> msigAccounts = { generateAccount(), generateAccount() };
    return msigAccounts;
  }

}

TestTransactionBuilder::TestTransactionBuilder() {
  tx = createTransaction();
}

TestTransactionBuilder::TestTransactionBuilder(const BinaryArray& txTemplate, const Crypto::SecretKey& secretKey) {
  tx = createTransaction(txTemplate);
  tx->setTransactionSecretKey(secretKey);
}

PublicKey TestTransactionBuilder::getTransactionPublicKey() const {
  return tx->getTransactionPublicKey();
}

void TestTransactionBuilder::setUnlockTime(uint64_t time) {
  tx->setUnlockTime(time);
}

size_t TestTransactionBuilder::addTestInput(uint64_t amount, const AccountKeys& senderKeys) {
  using namespace TransactionTypes;

  TransactionTypes::InputKeyInfo info;
  PublicKey targetKey;

  CryptoNote::KeyPair srcTxKeys = CryptoNote::generateKeyPair();
  derivePublicKey(senderKeys, srcTxKeys.publicKey, 5, targetKey);

  TransactionTypes::GlobalOutput gout = { targetKey, 0 };

  info.amount = amount;
  info.outputs.push_back(gout);

  info.realOutput.transactionIndex = 0;
  info.realOutput.outputInTransaction = 5;
  info.realOutput.transactionPublicKey = reinterpret_cast<const PublicKey&>(srcTxKeys.publicKey);

  KeyPair ephKeys;
  size_t idx = tx->addInput(senderKeys, info, ephKeys);
  keys[idx] = std::make_pair(info, ephKeys);
  return idx;
}

size_t TestTransactionBuilder::addTestInput(uint64_t amount, std::vector<uint32_t> gouts, const AccountKeys& senderKeys) {
  using namespace TransactionTypes;

  TransactionTypes::InputKeyInfo info;
  PublicKey targetKey;

  CryptoNote::KeyPair srcTxKeys = CryptoNote::generateKeyPair();
  derivePublicKey(senderKeys, srcTxKeys.publicKey, 5, targetKey);

  TransactionTypes::GlobalOutput gout = { targetKey, 0 };

  info.amount = amount;
  info.outputs.push_back(gout);
  PublicKey pk;
  SecretKey sk;
  for (auto out : gouts) {
    Crypto::generate_keys(pk, sk);
    info.outputs.push_back(TransactionTypes::GlobalOutput{ pk, out });
  }

  info.realOutput.transactionIndex = 0;
  info.realOutput.outputInTransaction = 5;
  info.realOutput.transactionPublicKey = reinterpret_cast<const PublicKey&>(srcTxKeys.publicKey);

  KeyPair ephKeys;
  size_t idx = tx->addInput(senderKeys, info, ephKeys);
  keys[idx] = std::make_pair(info, ephKeys);
  return idx;
}

void TestTransactionBuilder::addInput(const AccountKeys& senderKeys, const TransactionOutputInformation& t) {
  TransactionTypes::InputKeyInfo info;
  info.amount = t.amount;

  TransactionTypes::GlobalOutput globalOut;
  globalOut.outputIndex = t.globalOutputIndex;
  globalOut.targetKey = t.outputKey;
  info.outputs.push_back(globalOut);

  info.realOutput.outputInTransaction = t.outputInTransaction;
  info.realOutput.transactionIndex = 0;
  info.realOutput.transactionPublicKey = t.transactionPublicKey;

  KeyPair ephKeys;
  size_t idx = tx->addInput(senderKeys, info, ephKeys);
  keys[idx] = std::make_pair(info, ephKeys);
}

void TestTransactionBuilder::addTestMultisignatureInput(uint64_t amount, const TransactionOutputInformation& t) {
  MultisignatureInput input;
  input.amount = amount;
  input.outputIndex = t.globalOutputIndex;
  input.signatureCount = t.requiredSignatures;
  size_t idx = tx->addInput(input);
 
  msigInputs[idx] = MsigInfo{ t.transactionPublicKey, t.outputInTransaction, getMsigAccounts() };
}

size_t TestTransactionBuilder::addFakeMultisignatureInput(uint64_t amount, uint32_t globalOutputIndex, size_t signatureCount) {
  MultisignatureInput input;
  input.amount = amount;
  input.outputIndex = globalOutputIndex;
  input.signatureCount = signatureCount;
  size_t idx = tx->addInput(input);

  std::vector<AccountBase> accs;
  for (size_t i = 0; i < signatureCount; ++i) {
    accs.push_back(generateAccount());
  }

  msigInputs[idx] = MsigInfo{ Crypto::rand<PublicKey>(), 0, std::move(accs) };
  return idx;
}

TransactionOutputInformationIn TestTransactionBuilder::addTestKeyOutput(uint64_t amount, uint32_t globalOutputIndex, const AccountKeys& senderKeys) {
  uint32_t index = static_cast<uint32_t>(tx->addOutput(amount, senderKeys.address));

  uint64_t amount_;
  KeyOutput output;
  tx->getOutput(index, output, amount_);

  TransactionOutputInformationIn outputInfo;
  outputInfo.type = TransactionTypes::OutputType::Key;
  outputInfo.amount = amount_;
  outputInfo.globalOutputIndex = globalOutputIndex;
  outputInfo.outputInTransaction = index;
  outputInfo.transactionPublicKey = tx->getTransactionPublicKey();
  outputInfo.outputKey = output.key;
  outputInfo.keyImage = generateKeyImage(senderKeys, index, tx->getTransactionPublicKey());

  return outputInfo;
}

TransactionOutputInformationIn TestTransactionBuilder::addTestMultisignatureOutput(uint64_t amount, std::vector<AccountPublicAddress>& addresses, uint32_t globalOutputIndex) {
  uint32_t index = static_cast<uint32_t>(tx->addOutput(amount, addresses, static_cast<uint32_t>(addresses.size())));

  uint64_t _amount;
  MultisignatureOutput output;
  tx->getOutput(index, output, _amount);

  TransactionOutputInformationIn outputInfo;
  outputInfo.type = TransactionTypes::OutputType::Multisignature;
  outputInfo.amount = _amount;
  outputInfo.globalOutputIndex = globalOutputIndex;
  outputInfo.outputInTransaction = index;
  outputInfo.transactionPublicKey = tx->getTransactionPublicKey();
  // Doesn't used in multisignature output, so can contain garbage
  outputInfo.keyImage = generateKeyImage();
  outputInfo.requiredSignatures = output.requiredSignatureCount;
  return outputInfo;
}

TransactionOutputInformationIn TestTransactionBuilder::addTestMultisignatureOutput(uint64_t amount, uint32_t globalOutputIndex) {
  std::vector<AccountPublicAddress> multisigAddresses;
  for (const auto& acc : getMsigAccounts()) {
    multisigAddresses.push_back(acc.getAccountKeys().address);
  }

  return addTestMultisignatureOutput(amount, multisigAddresses, globalOutputIndex);
}

size_t TestTransactionBuilder::addOutput(uint64_t amount, const AccountPublicAddress& to) {
  return tx->addOutput(amount, to);
}

size_t TestTransactionBuilder::addOutput(uint64_t amount, const KeyOutput& out) {
  return tx->addOutput(amount, out);
}

size_t TestTransactionBuilder::addOutput(uint64_t amount, const MultisignatureOutput& out) {
  return tx->addOutput(amount, out);
}

std::unique_ptr<ITransactionReader> TestTransactionBuilder::build() {
  for (const auto& kv : keys) {
    tx->signInputKey(kv.first, kv.second.first, kv.second.second);
  }

  for (const auto& kv : msigInputs) {
    for (const auto& acc : kv.second.accounts) {
      tx->signInputMultisignature(kv.first, kv.second.transactionKey, kv.second.outputIndex, acc.getAccountKeys());
    }
  }

  transactionHash = tx->getTransactionHash();

  keys.clear();
  return std::move(tx);
}


Crypto::Hash TestTransactionBuilder::getTransactionHash() const {
  return transactionHash;
}
