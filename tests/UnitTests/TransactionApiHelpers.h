// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "CryptoTypes.h"
#include "ITransaction.h"
#include "crypto/crypto.h"

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

#include "Transfers/TransfersContainer.h"

namespace {

  using namespace CryptoNote;
  using namespace Crypto;

  inline AccountKeys accountKeysFromKeypairs(
    const KeyPair& viewKeys, 
    const KeyPair& spendKeys) {
    AccountKeys ak;
    ak.address.spendPublicKey = spendKeys.publicKey;
    ak.address.viewPublicKey = viewKeys.publicKey;
    ak.spendSecretKey = spendKeys.secretKey;
    ak.viewSecretKey = viewKeys.secretKey;
    return ak;
  }

  inline AccountKeys generateAccountKeys() {
    KeyPair p1;
    KeyPair p2;
    Crypto::generate_keys(p2.publicKey, p2.secretKey);
    Crypto::generate_keys(p1.publicKey, p1.secretKey);
    return accountKeysFromKeypairs(p1, p2);
  }

  AccountBase generateAccount() {
    AccountBase account;
    account.generate();
    return account;
  }

  AccountPublicAddress generateAddress() {
    return generateAccount().getAccountKeys().address;
  }
  
  KeyImage generateKeyImage() {
    return Crypto::rand<KeyImage>();
  }

  KeyImage generateKeyImage(const AccountKeys& keys, size_t idx, const PublicKey& txPubKey) {
    KeyImage keyImage;
    CryptoNote::KeyPair in_ephemeral;
    CryptoNote::generate_key_image_helper(
     keys,
      txPubKey,
      idx,
      in_ephemeral,
      keyImage);
    return keyImage;
  }

  void addTestInput(ITransaction& transaction, uint64_t amount) {
    KeyInput input;
    input.amount = amount;
    input.keyImage = generateKeyImage();
    input.outputIndexes.emplace_back(1);

    transaction.addInput(input);
  }

  TransactionOutputInformationIn addTestKeyOutput(ITransaction& transaction, uint64_t amount,
    uint32_t globalOutputIndex, const AccountKeys& senderKeys = generateAccountKeys()) {

    uint32_t index = static_cast<uint32_t>(transaction.addOutput(amount, senderKeys.address));

    uint64_t amount_;
    KeyOutput output;
    transaction.getOutput(index, output, amount_);

    TransactionOutputInformationIn outputInfo;
    outputInfo.type = TransactionTypes::OutputType::Key;
    outputInfo.amount = amount_;
    outputInfo.globalOutputIndex = globalOutputIndex;
    outputInfo.outputInTransaction = index;
    outputInfo.transactionPublicKey = transaction.getTransactionPublicKey();
    outputInfo.outputKey = output.key;
    outputInfo.keyImage = generateKeyImage(senderKeys, index, transaction.getTransactionPublicKey());

    return outputInfo;
  }

  inline Transaction convertTx(ITransactionReader& tx) {
    Transaction oldTx;
    fromBinaryArray(oldTx, tx.getTransactionData()); // ignore return code
    return oldTx;
  }
}

namespace CryptoNote {

class TestTransactionBuilder {
public:

  TestTransactionBuilder();
  TestTransactionBuilder(const BinaryArray& txTemplate, const Crypto::SecretKey& secretKey);

  PublicKey getTransactionPublicKey() const;
  void appendExtra(const BinaryArray& extraData);
  void setUnlockTime(uint64_t time);

  // inputs
  size_t addTestInput(uint64_t amount, const AccountKeys& senderKeys = generateAccountKeys());
  size_t addTestInput(uint64_t amount, std::vector<uint32_t> gouts, const AccountKeys& senderKeys = generateAccountKeys());
  void addTestMultisignatureInput(uint64_t amount, const TransactionOutputInformation& t);
  size_t addFakeMultisignatureInput(uint64_t amount, uint32_t globalOutputIndex, size_t signatureCount);
  void addInput(const AccountKeys& senderKeys, const TransactionOutputInformation& t);

  // outputs
  TransactionOutputInformationIn addTestKeyOutput(uint64_t amount, uint32_t globalOutputIndex, const AccountKeys& senderKeys = generateAccountKeys());
  TransactionOutputInformationIn addTestMultisignatureOutput(uint64_t amount, uint32_t globalOutputIndex);
  TransactionOutputInformationIn addTestMultisignatureOutput(uint64_t amount, std::vector<AccountPublicAddress>& addresses, uint32_t globalOutputIndex);
  size_t addOutput(uint64_t amount, const AccountPublicAddress& to);
  size_t addOutput(uint64_t amount, const KeyOutput& out);
  size_t addOutput(uint64_t amount, const MultisignatureOutput& out);

  // final step
  std::unique_ptr<ITransactionReader> build();

  // get built transaction hash (call only after build)
  Crypto::Hash getTransactionHash() const;

private:

  void derivePublicKey(const AccountKeys& reciever, const Crypto::PublicKey& srcTxKey, size_t outputIndex, PublicKey& ephemeralKey) {
    Crypto::KeyDerivation derivation;
    Crypto::generate_key_derivation(srcTxKey, reinterpret_cast<const Crypto::SecretKey&>(reciever.viewSecretKey), derivation);
    Crypto::derive_public_key(derivation, outputIndex,
      reinterpret_cast<const Crypto::PublicKey&>(reciever.address.spendPublicKey),
      reinterpret_cast<Crypto::PublicKey&>(ephemeralKey));
  }

  struct MsigInfo {
    PublicKey transactionKey;
    size_t outputIndex;
    std::vector<AccountBase> accounts;
  };

  std::unordered_map<size_t, std::pair<TransactionTypes::InputKeyInfo, KeyPair>> keys;
  std::unordered_map<size_t, MsigInfo> msigInputs;

  std::unique_ptr<ITransaction> tx;
  Crypto::Hash transactionHash;
};

class FusionTransactionBuilder {
public:
  FusionTransactionBuilder(const Currency& currency, uint64_t amount);

  uint64_t getAmount() const;
  void setAmount(uint64_t val);

  uint64_t getFirstInput() const;
  void setFirstInput(uint64_t val);

  uint64_t getFirstOutput() const;
  void setFirstOutput(uint64_t val);

  uint64_t getFee() const;
  void setFee(uint64_t val);

  size_t getExtraSize() const;
  void setExtraSize(size_t val);

  size_t getInputCount() const;
  void setInputCount(size_t val);

  std::unique_ptr<ITransactionReader> buildReader() const;
  Transaction buildTx() const;

  Transaction createFusionTransactionBySize(size_t targetSize);

private:
  const Currency& m_currency;
  uint64_t m_amount;
  uint64_t m_firstInput;
  uint64_t m_firstOutput;
  uint64_t m_fee;
  size_t m_extraSize;
  size_t m_inputCount;
};

}

namespace CryptoNote {
inline bool operator == (const AccountKeys& a, const AccountKeys& b) { 
  return memcmp(&a, &b, sizeof(a)) == 0; 
}
}
