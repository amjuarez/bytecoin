// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ITransaction.h"
#include "crypto/crypto.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "transfers/TransfersContainer.h"

namespace {

  using namespace CryptoNote;

  KeyPair generateKeys() {
    KeyPair kp;
    crypto::generate_keys(
      reinterpret_cast<crypto::public_key&>(kp.publicKey),
      reinterpret_cast<crypto::secret_key&>(kp.secretKey));
    return kp;
  }

  AccountKeys accountKeysFromKeypairs(const KeyPair& viewKeys, const KeyPair& spendKeys) {
    AccountKeys ak;
    ak.address.spendPublicKey = spendKeys.publicKey;
    ak.address.viewPublicKey = viewKeys.publicKey;
    ak.spendSecretKey = spendKeys.secretKey;
    ak.viewSecretKey = viewKeys.secretKey;
    return ak;
  }

  AccountKeys generateAccountKeys() {
    return accountKeysFromKeypairs(generateKeys(), generateKeys());
  }

  KeyImage generateKeyImage() {
    return crypto::rand<KeyImage>();
  }

  KeyImage generateKeyImage(const AccountKeys& keys, size_t idx, const PublicKey& txPubKey) {
    KeyImage keyImage;
    cryptonote::KeyPair in_ephemeral;
    cryptonote::generate_key_image_helper(
      reinterpret_cast<const cryptonote::account_keys&>(keys),
      reinterpret_cast<const crypto::public_key&>(txPubKey),
      idx,
      in_ephemeral,
      reinterpret_cast<crypto::key_image&>(keyImage));
    return keyImage;
  }

  void addTestInput(ITransaction& transaction, uint64_t amount) {
    TransactionTypes::InputKey input;
    input.amount = amount;
    input.keyImage = generateKeyImage();
    input.keyOffsets.emplace_back(1);

    transaction.addInput(input);
  }

  TransactionOutputInformationIn addTestKeyOutput(ITransaction& transaction, uint64_t amount,
    uint64_t globalOutputIndex, const AccountKeys& senderKeys = generateAccountKeys()) {

    uint32_t index = static_cast<uint32_t>(transaction.addOutput(amount, senderKeys.address));

    TransactionTypes::OutputKey output;
    transaction.getOutput(index, output);

    TransactionOutputInformationIn outputInfo;
    outputInfo.type = TransactionTypes::OutputType::Key;
    outputInfo.amount = output.amount;
    outputInfo.globalOutputIndex = globalOutputIndex;
    outputInfo.outputInTransaction = index;
    outputInfo.transactionPublicKey = transaction.getTransactionPublicKey();
    outputInfo.outputKey = output.key;
    outputInfo.keyImage = generateKeyImage(senderKeys, index, transaction.getTransactionPublicKey());

    return outputInfo;
  }


}

namespace CryptoNote {
inline bool operator == (const AccountKeys& a, const AccountKeys& b) { 
  return memcmp(&a, &b, sizeof(a)) == 0; 
}
}
