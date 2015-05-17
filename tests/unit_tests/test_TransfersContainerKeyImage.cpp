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

#include "gtest/gtest.h"

#include "IWallet.h"

#include "crypto/crypto.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/Currency.h"
#include "cryptonote_core/TransactionApi.h"
#include "transfers/TransfersContainer.h"

#include "TransactionApiHelpers.h"

using namespace CryptoNote;
using namespace cryptonote;


namespace {
  const size_t TEST_TRANSACTION_SPENDABLE_AGE = 1;
  const uint64_t TEST_OUTPUT_AMOUNT = 100;
  const uint64_t TEST_BLOCK_HEIGHT = 99;
  const uint64_t TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX = 113;
  const uint64_t UNCONFIRMED = UNCONFIRMED_TRANSACTION_HEIGHT;
  const uint64_t TEST_TIMESTAMP = 1000000;

  AccountAddress generateAddress() {
    account_base account;
    account.generate();
    return reinterpret_cast<const AccountAddress&>(account.get_keys().m_account_address);
  }

  void addInput(ITransaction& tx, const AccountKeys& senderKeys, const TransactionOutputInformation& t) {
    CryptoNote::KeyPair kp;
    TransactionTypes::InputKeyInfo info;
    info.amount = t.amount;

    TransactionTypes::GlobalOutput globalOut;
    globalOut.outputIndex = t.globalOutputIndex;
    globalOut.targetKey = t.outputKey;
    info.outputs.push_back(globalOut);

    info.realOutput.outputInTransaction = t.outputInTransaction;
    info.realOutput.transactionIndex = 0;
    info.realOutput.transactionPublicKey = t.transactionPublicKey;

    tx.addInput(senderKeys, info, kp);
  }

  TransactionOutputInformationIn addTestMultisignatureOutput(ITransaction& transaction, uint64_t amount,
                                                             uint64_t globalOutputIndex) {
    std::vector<AccountAddress> addresses;
    addresses.emplace_back(generateAddress());
    addresses.emplace_back(generateAddress());

    uint32_t index = static_cast<uint32_t>(transaction.addOutput(amount, addresses, static_cast<uint32_t>(addresses.size())));

    TransactionTypes::OutputMultisignature output;
    transaction.getOutput(index, output);

    TransactionOutputInformationIn outputInfo;
    outputInfo.type = TransactionTypes::OutputType::Multisignature;
    outputInfo.amount = output.amount;
    outputInfo.globalOutputIndex = globalOutputIndex;
    outputInfo.outputInTransaction = index;
    outputInfo.transactionPublicKey = transaction.getTransactionPublicKey();
    // Doesn't used in multisignature output, so can contain garbage
    outputInfo.keyImage = generateKeyImage();
    outputInfo.requiredSignatures = output.requiredSignatures;

    return outputInfo;
  }


  class TransfersContainerKeyImage : public ::testing::Test {
  public:

    TransfersContainerKeyImage() :
      currency(CurrencyBuilder().currency()), 
      container(currency, TEST_TRANSACTION_SPENDABLE_AGE), 
      account(generateAccountKeys()),
      txTemplate(createTransaction()) {
      txTemplate->getTransactionSecretKey(txSecretKey);
    }

  protected:

    std::vector<TransactionOutputInformation> getOutputs(uint32_t flags) {
      std::vector<TransactionOutputInformation> outs;
      container.getOutputs(outs, flags);
      return outs;
    }

    size_t outputsCount(uint32_t flags) {
      return getOutputs(flags).size();
    }

    BlockInfo blockInfo(uint64_t height) const {
      return BlockInfo{ height, 1000000 };
    }



    std::unique_ptr<ITransaction> createTransactionWithFixedKey() {
      auto tx = createTransaction(txTemplate->getTransactionData());
      tx->setTransactionSecretKey(txSecretKey);
      return tx;
    }

    std::unique_ptr<ITransaction> addTransactionWithFixedKey(uint64_t height, size_t inputs = 1, uint64_t amount = TEST_OUTPUT_AMOUNT, uint32_t txIndex = 0) {
      auto tx = createTransactionWithFixedKey();
      
      while (inputs--) {
        addTestInput(*tx, amount + 1);
      }

      auto outputIndex = (height == UNCONFIRMED_TRANSACTION_HEIGHT) ? 
        UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX : TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX;
      auto outInfo = addTestKeyOutput(*tx, amount, outputIndex, account);
      EXPECT_TRUE(container.addTransaction(BlockInfo{ height, 1000000, txIndex }, *tx, { outInfo }));
      return tx;
    }

    std::unique_ptr<ITransaction> addTransaction(uint64_t height = UNCONFIRMED_TRANSACTION_HEIGHT) {
      auto tx = createTransaction();
      addTestInput(*tx, TEST_OUTPUT_AMOUNT + 1);
      auto outputIndex = (height == UNCONFIRMED_TRANSACTION_HEIGHT) ? UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX : TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX;
      auto outInfo = addTestKeyOutput(*tx, TEST_OUTPUT_AMOUNT, outputIndex, account);
      std::vector<TransactionOutputInformationIn> outputs = { outInfo };
      EXPECT_TRUE(container.addTransaction(blockInfo(height), *tx, outputs));
      return tx;
    }

    std::unique_ptr<ITransaction> addSpendingTransaction(const Hash& sourceTx, uint64_t height, uint64_t outputIndex, uint64_t amount = TEST_OUTPUT_AMOUNT, bool fixedKey = false) {
      std::unique_ptr<ITransaction> tx;
      auto outputs = container.getTransactionOutputs(sourceTx, ITransfersContainer::IncludeTypeAll |
        ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeStateSoftLocked);

      EXPECT_FALSE(outputs.empty());

      if (outputs.empty())
        return tx;

      tx = fixedKey ? createTransactionWithFixedKey() : createTransaction();

      size_t inputAmount = 0;
      for (const auto& t : outputs) {
        inputAmount += t.amount;
        addInput(*tx, account, t);
      }

      EXPECT_GE(inputAmount, amount);

      std::vector<TransactionOutputInformationIn> transfers;

      addTestKeyOutput(*tx, amount, outputIndex); // output to some random address

      if (inputAmount > amount) {
        transfers.emplace_back(addTestKeyOutput(*tx, inputAmount - amount, outputIndex + 1, account)); // change
      }

      EXPECT_TRUE(container.addTransaction(blockInfo(height), *tx, transfers));

      return tx;
    }

    Currency currency;
    TransfersContainer container;
    AccountKeys account;
    std::unique_ptr<ITransaction> txTemplate;
    SecretKey txSecretKey;
  };

}

/////////////////////////////////////////////////////////////////////////////
// addTransaction
/////////////////////////////////////////////////////////////////////////////

TEST_F(TransfersContainerKeyImage, addTransaction_addingSecondUnconfirmedTransferHidesBothUnconfirmedTransfers) {
  // add first transaction
  auto tx1 = createTransactionWithFixedKey();
  addTestInput(*tx1, TEST_OUTPUT_AMOUNT);
  auto tx1out = addTestKeyOutput(*tx1, TEST_OUTPUT_AMOUNT, UNCONFIRMED, account);

  ASSERT_TRUE(container.addTransaction({ UNCONFIRMED, 100000 }, *tx1, { tx1out }));
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(1, outputsCount(ITransfersContainer::IncludeAllLocked));
  
  auto tx2 = createTransactionWithFixedKey();
  ASSERT_EQ(tx1->getTransactionPublicKey(), tx2->getTransactionPublicKey());

  // fill tx2
  addTestInput(*tx2, TEST_OUTPUT_AMOUNT);
  addTestInput(*tx2, TEST_OUTPUT_AMOUNT);
  auto tx2out = addTestKeyOutput(*tx2, TEST_OUTPUT_AMOUNT, UNCONFIRMED, account);

  ASSERT_EQ(tx1out.keyImage, tx2out.keyImage);
  ASSERT_NE(tx1->getTransactionPrefixHash(), tx2->getTransactionPrefixHash());

  ASSERT_TRUE(container.addTransaction({ UNCONFIRMED, 100000 }, *tx2, { tx2out }));

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked)); // transactions' outputs should shadow one another
  ASSERT_EQ(0, outputsCount(ITransfersContainer::IncludeAllLocked));
}

TEST_F(TransfersContainerKeyImage, addTransaction_unconfirmedTransferAddedAfterConfirmedBecomeHidden) {
  // fill tx1
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(1, outputsCount(ITransfersContainer::IncludeAllUnlocked));

  auto tx2 = createTransactionWithFixedKey();
  ASSERT_EQ(tx1->getTransactionPublicKey(), tx2->getTransactionPublicKey());

  // fill tx2
  addTestInput(*tx2, TEST_OUTPUT_AMOUNT);
  addTestInput(*tx2, TEST_OUTPUT_AMOUNT);
  auto tx2out = addTestKeyOutput(*tx2, TEST_OUTPUT_AMOUNT, UNCONFIRMED, account);

  ASSERT_TRUE(container.addTransaction({ UNCONFIRMED, 100000 }, *tx2, { tx2out }));

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(1, outputsCount(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(0, outputsCount(ITransfersContainer::IncludeAllLocked));
}


TEST_F(TransfersContainerKeyImage, addTransaction_unconfirmedTransferAddedAfterSpentBecomeHidden) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, 
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(0, outputsCount(ITransfersContainer::IncludeAll));

  addTransactionWithFixedKey(UNCONFIRMED, 2);

  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(0, outputsCount(ITransfersContainer::IncludeAll));
}


TEST_F(TransfersContainerKeyImage, addTransaction_confirmedTransferAddedAfterUnconfirmedHidesUnconfirmed) {
  auto tx1 = addTransactionWithFixedKey(UNCONFIRMED);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, container.balance(ITransfersContainer::IncludeAllUnlocked)); // confirmed added
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked)); // unconfirmed shadowed
}

TEST_F(TransfersContainerKeyImage, addTransaction_secondConfirmedTransferAddedAsHidden_BothTransfersInTheSameBlock) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT, 1);
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2, 2);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainerKeyImage, addTransaction_secondConfirmedTransferAddedAsHidden_TransfersInDifferentBlocks) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  container.advanceHeight(TEST_BLOCK_HEIGHT + 1 + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainerKeyImage, addTransaction_confirmedTransferAddedAfterSpentBecomeHidden) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + 1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll)); // all spent

  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll)); // all spent
}

/////////////////////////////////////////////////////////////////////////////
// markTransactionConfirmed
/////////////////////////////////////////////////////////////////////////////

TEST_F(TransfersContainerKeyImage, markTransactionConfirmed_confirmingOneOfAFewUnconfirmedTransfersMakesThisTransferVisible) {
  addTransactionWithFixedKey(UNCONFIRMED, 1, TEST_OUTPUT_AMOUNT);
  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);
  addTransactionWithFixedKey(UNCONFIRMED, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.markTransactionConfirmed(
    { TEST_BLOCK_HEIGHT, 100000 }, tx2->getTransactionHash(), { TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX }));

  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainerKeyImage, markTransactionConfirmed_oneConfirmedOtherUnconfirmed_confirmingOneUnconfirmed) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT, 1);
  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);
  addTransactionWithFixedKey(UNCONFIRMED, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll));

  // same block, but smaller transactionIndex
  ASSERT_TRUE(container.markTransactionConfirmed(
    { TEST_BLOCK_HEIGHT, TEST_TIMESTAMP, 2 }, tx2->getTransactionHash(), { TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX }));

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, markTransactionConfirmed_oneSpentOtherUnconfirmed_confirmingOneUnconfirmed) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, 
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(UNCONFIRMED, 3, TEST_OUTPUT_AMOUNT * 3);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.markTransactionConfirmed(
  { TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1, TEST_TIMESTAMP, 0 }, tx2->getTransactionHash(), {TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX}));

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

/////////////////////////////////////////////////////////////////////////////
// addTransaction - spending fails
/////////////////////////////////////////////////////////////////////////////

class TransfersContainerKeyImage_Spend : public TransfersContainerKeyImage {
public:

  bool spendOutput(const TransactionOutputInformation& outInfo) {
    auto amount = outInfo.amount;
    auto spendTx = createTransaction();
    addInput(*spendTx, account, outInfo);
    addTestKeyOutput(*spendTx, amount, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
    return container.addTransaction({ TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, TEST_TIMESTAMP, 0 }, *spendTx, {});
  }
};


TEST_F(TransfersContainerKeyImage_Spend, spendingKeyImageWithWrongAmountCausesException) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);

  auto outputs = getOutputs(ITransfersContainer::IncludeTypeAll | ITransfersContainer::IncludeStateSoftLocked);

  ASSERT_EQ(1, outputs.size());
  // mess with amount
  outputs[0].amount = TEST_OUTPUT_AMOUNT * 2;
  ASSERT_ANY_THROW(spendOutput(outputs[0]));
  ASSERT_EQ(1, container.transactionsCount());
}

TEST_F(TransfersContainerKeyImage_Spend, spendUnconfirmedeKeyImageCausesException) {
  auto tx1 = addTransactionWithFixedKey(UNCONFIRMED);
  auto outputs = getOutputs(ITransfersContainer::IncludeAllLocked);
  ASSERT_FALSE(outputs.empty());

  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_ANY_THROW(spendOutput(outputs[0]));
}

TEST_F(TransfersContainerKeyImage_Spend, spendingUnconfirmedTransferIfThereIsConfirmedWithAnotherAmountCausesException) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(2, container.transactionsCount());

  auto outputs = getOutputs(ITransfersContainer::IncludeTypeAll | ITransfersContainer::IncludeStateSoftLocked);
  ASSERT_FALSE(outputs.empty());
  outputs[0].amount = TEST_OUTPUT_AMOUNT * 2;

  ASSERT_ANY_THROW(spendOutput(outputs[0]));
}

TEST_F(TransfersContainerKeyImage_Spend, spendingTransferIfThereIsSpentTransferWithAnotherAmountCausesException) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto outputs = container.getTransactionOutputs(tx1->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_EQ(1, outputs.size());

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  addSpendingTransaction(tx1->getTransactionHash(), 
    TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_ANY_THROW(spendOutput(outputs[0]));
}


/////////////////////////////////////////////////////////////////////////////
// addTransaction - spending succeeds
/////////////////////////////////////////////////////////////////////////////
TEST_F(TransfersContainerKeyImage_Spend, spendingVisibleConfirmedTransfer) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_NE(tx1->getTransactionHash(), tx2->getTransactionHash());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  // spend first confirmed transaction
  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto spentOutputs = container.getSpentOutputs();
  ASSERT_EQ(1, spentOutputs.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, spentOutputs[0].amount);
  ASSERT_EQ(tx1->getTransactionHash(), spentOutputs[0].transactionHash);
}

TEST_F(TransfersContainerKeyImage_Spend, spendHiddenConfirmedTransfer) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_NE(tx1->getTransactionHash(), tx2->getTransactionHash());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  // spend second confirmed transaction (hidden)
  addSpendingTransaction(tx2->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto spentOutputs = container.getSpentOutputs();
  ASSERT_EQ(1, spentOutputs.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, spentOutputs[0].amount);
  ASSERT_EQ(tx2->getTransactionHash(), spentOutputs[0].transactionHash);
}

TEST_F(TransfersContainerKeyImage_Spend, spendSecondHiddenConfirmedOutput) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 2, 3, TEST_OUTPUT_AMOUNT * 3);

  // spend third confirmed transaction (hidden)
  addSpendingTransaction(tx3->getTransactionHash(), TEST_BLOCK_HEIGHT + 2 + TEST_TRANSACTION_SPENDABLE_AGE,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto spentOutputs = container.getSpentOutputs();
  ASSERT_EQ(1, spentOutputs.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 3, spentOutputs[0].amount);
  ASSERT_EQ(tx3->getTransactionHash(), spentOutputs[0].transactionHash);
}

TEST_F(TransfersContainerKeyImage_Spend, spendHiddenWithSameAmount_oneBlock) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT - 1);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2, 1);
  auto tx3 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 3, TEST_OUTPUT_AMOUNT * 2, 2);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  addSpendingTransaction(tx3->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto spentOutputs = container.getSpentOutputs();
  ASSERT_EQ(1, spentOutputs.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, spentOutputs[0].amount);
  ASSERT_EQ(tx2->getTransactionHash(), spentOutputs[0].transactionHash);
}

TEST_F(TransfersContainerKeyImage_Spend, spendHiddenWithSameAmount_differentBlocks) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 3, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 2, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  addSpendingTransaction(tx3->getTransactionHash(), TEST_BLOCK_HEIGHT + 2 + TEST_TRANSACTION_SPENDABLE_AGE,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto spentOutputs = container.getSpentOutputs();
  ASSERT_EQ(1, spentOutputs.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, spentOutputs[0].amount);
  ASSERT_EQ(tx2->getTransactionHash(), spentOutputs[0].transactionHash);
}

/////////////////////////////////////////////////////////////////////////////
// remove spending transaction
/////////////////////////////////////////////////////////////////////////////
class TransfersContainerKeyImage_Remove : public TransfersContainerKeyImage {
public: 
  void checkSpentOutputs(const Hash& expectedTxHash) {
    auto spentOutputs = container.getSpentOutputs();
    ASSERT_EQ(1, spentOutputs.size());
    ASSERT_EQ(expectedTxHash, spentOutputs[0].transactionHash);
  }
};

TEST_F(TransfersContainerKeyImage_Remove, removeUnconfirmedTxSpendingVisibleOutput) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT, 1);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2, 2);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  auto spendingTx = addSpendingTransaction(tx1->getTransactionHash(),
    UNCONFIRMED, UNCONFIRMED, TEST_OUTPUT_AMOUNT);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_NO_FATAL_FAILURE(checkSpentOutputs(tx1->getTransactionHash()));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(spendingTx->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage_Remove, removeUnconfirmedTxSpendingHiddenOut) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT, 1);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2, 2);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  auto spendingTx = addSpendingTransaction(tx2->getTransactionHash(),
    UNCONFIRMED, UNCONFIRMED, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_NO_FATAL_FAILURE(checkSpentOutputs(tx2->getTransactionHash()));
  ASSERT_TRUE(container.deleteUnconfirmedTransaction(spendingTx->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage_Remove, removeUnconfirmedTxAfterAddingMoreTx) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT, 1);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
  auto spendingTx = addSpendingTransaction(tx1->getTransactionHash(),
    UNCONFIRMED, UNCONFIRMED, TEST_OUTPUT_AMOUNT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(spendingTx->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

/////////////////////////////////////////////////////////////////////////////
// remove unconfirmed output
/////////////////////////////////////////////////////////////////////////////
TEST_F(TransfersContainerKeyImage, removingOneOfTwoUnconfirmedTransfersMakesAnotherVisible) {
  auto tx1 = addTransactionWithFixedKey(UNCONFIRMED);
  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2, 2);
  
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx2->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removingOneOfThreeUnconfirmedTransfersDoesNotMakeVisibleAnyOfRemaining) {
  auto tx1 = addTransactionWithFixedKey(UNCONFIRMED);
  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(UNCONFIRMED, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx2->getTransactionHash()));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removingOneOfTwoUnconfirmedTransfersIfThereIsConfirmedTransferDoesNotAffectBalance) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(UNCONFIRMED, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx2->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removingOnlyUnconfirmedTransfersIfThereIsConfirmedTransferDoesNotAffectBalance) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx2->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removingOneOfTwoUnconfirmedTransfersIfThereIsSpentTransferDoesNotAffectBalance) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(UNCONFIRMED, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  auto spendingTx = addSpendingTransaction(tx1->getTransactionHash(),
    TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx2->getTransactionHash()));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

/////////////////////////////////////////////////////////////////////////////
// remove confirmed output
/////////////////////////////////////////////////////////////////////////////
TEST_F(TransfersContainerKeyImage, removeConfirmed_oneOfThree) {
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 2, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  container.detach(TEST_BLOCK_HEIGHT + 2);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_oneOfTwo) {
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  container.detach(TEST_BLOCK_HEIGHT + 1);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_revealsUnconfirmed) {
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_EQ(1, container.detach(TEST_BLOCK_HEIGHT).size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, container.balance(ITransfersContainer::IncludeAllLocked));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_twoUnconfirmedHidden) {
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);
  addTransactionWithFixedKey(UNCONFIRMED, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_EQ(1, container.detach(TEST_BLOCK_HEIGHT).size());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_twoConfirmedOneUnconfirmedHidden) {
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  addTransactionWithFixedKey(UNCONFIRMED, 3, TEST_OUTPUT_AMOUNT * 3);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_EQ(1, container.detach(TEST_BLOCK_HEIGHT + 1).size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_oneSpentOneConfirmed) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + 1,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_EQ(1, container.detach(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1).size());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_oneSpentTwoConfirmed) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + 1,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 2, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(4, container.transactionsCount());

  ASSERT_EQ(1, container.detach(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 2).size());
  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}
