// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include <cryptonote_config.h>
#include <ITransaction.h>
#include <cryptonote_core/account.h>
#include <cryptonote_core/cryptonote_basic.h>
#include <cryptonote_core/cryptonote_format_utils.h>
#include <cryptonote_core/TransactionApi.h>
#include <cryptonote_protocol/blobdatatype.h>

using namespace cryptonote;
using namespace CryptoNote;

class CryptoNoteBasicTest : public testing::Test {
public:
  CryptoNoteBasicTest() : transaction(createTransaction()) {
    acc.generate();
    key.generate();
    transaction->addInput(TransactionTypes::InputMultisignature{6, 1, 1, 10});
    transaction->addInput(TransactionTypes::InputMultisignature{10, 1, 2, 10});
    const AccountAddress& addr = reinterpret_cast<const AccountAddress&>(acc.get_keys().m_account_address);
    transaction->addOutput(10, {addr}, 1, 1);
    transaction->signInputMultisignature(
        0, reinterpret_cast<const PublicKey&>(key.get_keys().m_account_address.m_viewPublicKey), 1,
        reinterpret_cast<const AccountKeys&>(acc.get_keys()));
    transaction->signInputMultisignature(
        1, reinterpret_cast<const PublicKey&>(key.get_keys().m_account_address.m_viewPublicKey), 2,
        reinterpret_cast<const AccountKeys&>(acc.get_keys()));
  }
  account_base acc;
  account_base key;
  std::unique_ptr<ITransaction> transaction;
  Transaction unpacked;
};

TEST_F(CryptoNoteBasicTest, transactionWithDepositsSerializationAndDeserialization) {
  auto blob = transaction->getTransactionData();
  ASSERT_TRUE(parse_and_validate_tx_from_blob(std::string(blob.begin(), blob.end()), unpacked));
  ASSERT_TRUE(transaction->getTransactionHash() == createTransaction(unpacked)->getTransactionHash());
}
