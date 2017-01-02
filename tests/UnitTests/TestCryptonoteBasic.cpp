// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include <CryptoNoteConfig.h>
#include <ITransaction.h>
#include <CryptoNoteCore/CryptoNoteBasic.h>
#include <CryptoNoteCore/Account.h>
#include <CryptoNoteCore/CryptoNoteFormatUtils.h>
#include <CryptoNoteCore/TransactionApi.h>

using namespace CryptoNote;

class CryptoNoteBasicTest : public testing::Test {
public:
  CryptoNoteBasicTest() : transaction(createTransaction()) {
    acc.generate();
    key.generate();
    transaction->addInput(MultisignatureInput{ 6, 1, 1, 10 });
    transaction->addInput(MultisignatureInput{ 10, 1, 2, 10 });
    const auto& addr = acc.getAccountKeys().address;
    transaction->addOutput(10, {addr}, 1, 1);
    transaction->signInputMultisignature(0, key.getAccountKeys().address.viewPublicKey, 1, acc.getAccountKeys());
    transaction->signInputMultisignature(1, key.getAccountKeys().address.viewPublicKey, 2, acc.getAccountKeys());
  }

  AccountBase acc;
  AccountBase key;
  std::unique_ptr<ITransaction> transaction;
  Transaction unpacked;
};

TEST_F(CryptoNoteBasicTest, transactionWithDepositsSerializationAndDeserialization) {
  Crypto::Hash ignore1;
  Crypto::Hash ignore2;
  ASSERT_TRUE(parseAndValidateTransactionFromBinaryArray(transaction->getTransactionData(), unpacked, ignore1, ignore2));
  ASSERT_TRUE(transaction->getTransactionHash() == createTransaction(unpacked)->getTransactionHash());
}
