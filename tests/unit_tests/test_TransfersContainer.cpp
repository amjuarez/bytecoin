// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

  void addMultisignatureInput(ITransaction& tx, uint64_t amount, uint32_t signatures, uint64_t outputIndex, uint32_t term) {
    TransactionTypes::InputMultisignature input;
    input.amount = amount;
    input.signatures = signatures;
    input.outputIndex = outputIndex;
    input.term = term;

    tx.addInput(input);
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
    outputInfo.term = output.term;

    return outputInfo;
  }


  class TransfersContainerTest : public ::testing::Test {
  public:
    enum : uint64_t {
      TEST_CONTAINER_CURRENT_HEIGHT = 1000
    };

    TransfersContainerTest() : 
      currency(CurrencyBuilder().currency()), 
      container(currency, TEST_TRANSACTION_SPENDABLE_AGE), 
      account(generateAccountKeys()) {   
    }

  protected:

    BlockInfo blockInfo(uint64_t height) const {
      return BlockInfo{ height, 1000000 };
    }

    std::unique_ptr<ITransaction> addTransaction(uint64_t height = UNCONFIRMED_TRANSACTION_HEIGHT,
                                                 uint64_t outputAmount = TEST_OUTPUT_AMOUNT, std::vector<TransactionOutputInformation>* unlockedTransfers = nullptr) {
      auto tx = createTransaction();
      addTestInput(*tx, outputAmount + 1);
      auto outputIndex = (height == UNCONFIRMED_TRANSACTION_HEIGHT) ? UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX : TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX;
      auto outInfo = addTestKeyOutput(*tx, outputAmount, outputIndex, account);
      std::vector<TransactionOutputInformationIn> outputs = { outInfo };
      EXPECT_TRUE(container.addTransaction(blockInfo(height), *tx, outputs, {}, unlockedTransfers));
      return tx;
    }

    std::unique_ptr<ITransaction> addSpendingTransaction(const Hash& sourceTx, uint64_t height, uint64_t outputIndex, uint64_t amount = TEST_OUTPUT_AMOUNT,
                                                         std::vector<TransactionOutputInformation>* unlockedTransfers = nullptr) {
      std::unique_ptr<ITransaction> tx;
      auto outputs = container.getTransactionOutputs(sourceTx, ITransfersContainer::IncludeTypeAll |
        ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeStateSoftLocked);

      EXPECT_FALSE(outputs.empty());

      if (outputs.empty())
        return tx;

      tx = createTransaction();

      size_t inputAmount = 0;
      for (const auto& t : outputs) {
        inputAmount += t.amount;
        if (t.type == TransactionTypes::OutputType::Key) {
          addInput(*tx, account, t);
        } else if (t.type == TransactionTypes::OutputType::Multisignature) {
          addMultisignatureInput(*tx, t.amount, t.requiredSignatures, t.globalOutputIndex, t.term);
        }
      }

      EXPECT_GE(inputAmount, amount);

      std::vector<TransactionOutputInformationIn> transfers;

      addTestKeyOutput(*tx, amount, outputIndex); // output to some random address

      if (inputAmount > amount) {
        transfers.emplace_back(addTestKeyOutput(*tx, inputAmount - amount, outputIndex + 1, account)); // change
      }

      EXPECT_TRUE(container.addTransaction(blockInfo(height), *tx, transfers, {}, unlockedTransfers));

      return tx;
    }

    TransactionOutputInformationIn addDepositOutput(ITransaction& tx, uint64_t amount, uint32_t term, uint64_t height, uint32_t requiredSignatures = 1) {
      auto index = tx.addOutput(amount, {account.address}, requiredSignatures, term);
      auto outputIndex = (height == UNCONFIRMED_TRANSACTION_HEIGHT) ? UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX : TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX;

      TransactionTypes::OutputMultisignature output;
      tx.getOutput(index, output);

      TransactionOutputInformationIn outputInfo;
      outputInfo.type = TransactionTypes::OutputType::Multisignature;
      outputInfo.amount = output.amount;
      outputInfo.globalOutputIndex = outputIndex;
      outputInfo.outputInTransaction = index;
      outputInfo.transactionPublicKey = tx.getTransactionPublicKey();
      outputInfo.transactionHash = tx.getTransactionHash();
      outputInfo.requiredSignatures = requiredSignatures;
      outputInfo.term = term;
      outputInfo.keyImage = generateKeyImage(account, index, tx.getTransactionPublicKey());

      return outputInfo;
    }

    std::vector<Hash> detachContainer(uint64_t height) {
      std::vector<Hash> deletedTransactions;
      std::vector<TransactionOutputInformation> lockedTransfers;

      container.detach(height, deletedTransactions, lockedTransfers);
      return deletedTransactions;
    }

    std::vector<TransactionOutputInformation> detachContainerGetLockedTransfers(uint64_t height) {
      std::vector<Hash> deletedTransactions;
      std::vector<TransactionOutputInformation> lockedTransfers;

      container.detach(height, deletedTransactions, lockedTransfers);
      return lockedTransfers;
    }

    Currency currency;
    TransfersContainer container;
    AccountKeys account;
  };

}

//--------------------------------------------------------------------------- 
// TransfersContainer_addTransaction
//--------------------------------------------------------------------------- 
class TransfersContainer_addTransaction : public TransfersContainerTest {};

TEST_F(TransfersContainer_addTransaction, orderIsRequired_sameHeight) {
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
}

TEST_F(TransfersContainer_addTransaction, orderIsRequired_confirmed) {
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT + 1));
  ASSERT_ANY_THROW(addTransaction(TEST_BLOCK_HEIGHT));
}

TEST_F(TransfersContainer_addTransaction, orderIsRequired_unconfirmedAtAnyHeight) {
  ASSERT_NO_THROW(addTransaction(UNCONFIRMED_TRANSACTION_HEIGHT));
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
  ASSERT_NO_THROW(addTransaction(UNCONFIRMED_TRANSACTION_HEIGHT));
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT + 1));
  ASSERT_NO_THROW(addTransaction(UNCONFIRMED_TRANSACTION_HEIGHT));
}

TEST_F(TransfersContainer_addTransaction, orderIsRequired_afterDetach) {
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT + 1));
  detachContainer(TEST_BLOCK_HEIGHT + 1);
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
}


TEST_F(TransfersContainer_addTransaction, addingTransactionTwiceCausesException) {
  auto tx = createTransaction();
  addTestInput(*tx, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo = addTestKeyOutput(*tx, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, { outInfo }, {}));
  ASSERT_ANY_THROW(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT + 1), *tx, { outInfo }, {}));
}

TEST_F(TransfersContainer_addTransaction, addingTwoIdenticalUnconfirmedMultisignatureOutputsDoesNotCauseException) {

  CryptoNote::BlockInfo blockInfo{ UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };

  auto tx1 = createTransaction();
  addTestInput(*tx1, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo1 = addTestMultisignatureOutput(*tx1, TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs1;
  outputs1.emplace_back(outInfo1);

  ASSERT_TRUE(container.addTransaction(blockInfo, *tx1, outputs1, {}));

  auto tx2 = createTransaction();
  addTestInput(*tx2, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo2 = addTestMultisignatureOutput(*tx2, TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs2;
  outputs2.emplace_back(outInfo2);

  ASSERT_TRUE(container.addTransaction(blockInfo, *tx2, outputs2, {}));

  container.advanceHeight(1000);

  ASSERT_EQ(2, container.transfersCount());
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(2 * TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, addingConfirmedMultisignatureOutputIdenticalAnotherUnspentOuputCausesException) {
  CryptoNote::BlockInfo blockInfo{ TEST_BLOCK_HEIGHT, 1000000 };

  auto tx1 = createTransaction();
  addTestInput(*tx1, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo1 = addTestMultisignatureOutput(*tx1, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs1;
  outputs1.emplace_back(outInfo1);

  ASSERT_TRUE(container.addTransaction(blockInfo, *tx1, outputs1, {}));

  auto tx2 = createTransaction();
  addTestInput(*tx2, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo2 = addTestMultisignatureOutput(*tx2, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs2;
  outputs2.emplace_back(outInfo2);

  ASSERT_ANY_THROW(container.addTransaction(blockInfo, *tx2, outputs2, {}));

  container.advanceHeight(1000);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, addingConfirmedMultisignatureOutputIdenticalAnotherSpentOuputCausesException) {
  CryptoNote::BlockInfo blockInfo1{ TEST_BLOCK_HEIGHT, 1000000 };
  auto tx1 = createTransaction();
  addTestInput(*tx1, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo1 = addTestMultisignatureOutput(*tx1, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs1;
  outputs1.emplace_back(outInfo1);

  ASSERT_TRUE(container.addTransaction(blockInfo1, *tx1, outputs1, {}));

  // Spend output
  CryptoNote::BlockInfo blockInfo2{ UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };
  auto tx2 = createTransaction();
  TransactionTypes::InputMultisignature input2;
  input2.amount = TEST_OUTPUT_AMOUNT;
  input2.outputIndex = outInfo1.globalOutputIndex;
  input2.signatures = outInfo1.requiredSignatures;
  tx2->addInput(input2);
  ASSERT_TRUE(container.addTransaction(blockInfo2, *tx2, std::vector<TransactionOutputInformationIn>(), {}));

  CryptoNote::BlockInfo blockInfo3{ TEST_BLOCK_HEIGHT + 3, 1000000 };
  auto tx3 = createTransaction();
  addTestInput(*tx3, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo3 = addTestMultisignatureOutput(*tx3, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs3;
  outputs3.emplace_back(outInfo3);

  ASSERT_ANY_THROW(container.addTransaction(blockInfo3, *tx3, outputs3, {}));

  container.advanceHeight(1000);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, addingConfirmedBlockAndUnconfirmedOutputCausesException) {
  CryptoNote::BlockInfo blockInfo{ TEST_BLOCK_HEIGHT, 1000000 };

  auto tx = createTransaction();
  addTestInput(*tx, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo = addTestKeyOutput(*tx, TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs;
  outputs.emplace_back(outInfo);

  ASSERT_ANY_THROW(container.addTransaction(blockInfo, *tx, outputs, {}));
}

TEST_F(TransfersContainer_addTransaction, addingUnconfirmedBlockAndConfirmedOutputCausesException) {
  CryptoNote::BlockInfo blockInfo{ UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };

  auto tx = createTransaction();
  addTestInput(*tx, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo = addTestKeyOutput(*tx, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs;
  outputs.emplace_back(outInfo);

  ASSERT_ANY_THROW(container.addTransaction(blockInfo, *tx, outputs, {}));
}

TEST_F(TransfersContainer_addTransaction, handlesAddingUnconfirmedOutputToKey) {
  CryptoNote::BlockInfo blockInfo{ UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };

  auto tx = createTransaction();
  addTestInput(*tx, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo = addTestKeyOutput(*tx, TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs;
  outputs.emplace_back(outInfo);

  ASSERT_TRUE(container.addTransaction(blockInfo, *tx, outputs, {}));

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllLocked);
  ASSERT_EQ(1, transfers.size());

  transfers.clear();
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);
  ASSERT_TRUE(transfers.empty());

  transfers = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAllLocked);
  ASSERT_EQ(1, transfers.size());

  transfers = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAllUnlocked);
  ASSERT_TRUE(transfers.empty());

  TransactionInformation txInfo;
  int64_t txBalance;
  ASSERT_TRUE(container.getTransactionInformation(tx->getTransactionHash(), txInfo, txBalance));
  ASSERT_EQ(blockInfo.height, txInfo.blockHeight);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, txBalance);

  std::vector<crypto::hash> unconfirmedTransactions;
  container.getUnconfirmedTransactions(unconfirmedTransactions);
  ASSERT_EQ(1, unconfirmedTransactions.size());
}

TEST_F(TransfersContainer_addTransaction, handlesAddingConfirmedOutputToKey) {
  CryptoNote::BlockInfo blockInfo{ TEST_BLOCK_HEIGHT, 1000000 };

  auto tx = createTransaction();
  addTestInput(*tx, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo = addTestKeyOutput(*tx, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs;
  outputs.emplace_back(outInfo);

  ASSERT_TRUE(container.addTransaction(blockInfo, *tx, outputs, {}));

  container.advanceHeight(1000);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllLocked);
  ASSERT_TRUE(transfers.empty());

  transfers.clear();
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);
  ASSERT_EQ(1, transfers.size());

  transfers = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAllLocked);
  ASSERT_TRUE(transfers.empty());

  transfers = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAllUnlocked);
  ASSERT_EQ(1, transfers.size());

  TransactionInformation txInfo;
  int64_t txBalance;
  ASSERT_TRUE(container.getTransactionInformation(tx->getTransactionHash(), txInfo, txBalance));
  ASSERT_EQ(blockInfo.height, txInfo.blockHeight);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, txBalance);

  std::vector<crypto::hash> unconfirmedTransactions;
  container.getUnconfirmedTransactions(unconfirmedTransactions);
  ASSERT_TRUE(unconfirmedTransactions.empty());
}

TEST_F(TransfersContainer_addTransaction, addingEmptyTransactionOuptutsDoesNotChaingeContainer) {
  CryptoNote::BlockInfo blockInfo{ UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };

  auto tx = createTransaction();
  addTestInput(*tx, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo = addTestKeyOutput(*tx, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);

  std::vector<TransactionOutputInformationIn> outputs;

  ASSERT_FALSE(container.addTransaction(blockInfo, *tx, outputs, {}));

  ASSERT_EQ(0, container.transfersCount());
  ASSERT_EQ(0, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllLocked);
  ASSERT_TRUE(transfers.empty());

  transfers.clear();
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);
  ASSERT_TRUE(transfers.empty());

  transfers = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_TRUE(transfers.empty());

  TransactionInformation txInfo;
  int64_t txBalance;
  ASSERT_FALSE(container.getTransactionInformation(tx->getTransactionHash(), txInfo, txBalance));

  std::vector<crypto::hash> unconfirmedTransactions;
  container.getUnconfirmedTransactions(unconfirmedTransactions);
  ASSERT_TRUE(unconfirmedTransactions.empty());
}


TEST_F(TransfersContainer_addTransaction, handlesAddingUnconfirmedOutputMultisignature) {
  auto tx = createTransaction();
  auto out = addTestMultisignatureOutput(*tx, TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

  ASSERT_TRUE(container.addTransaction(blockInfo(UNCONFIRMED_TRANSACTION_HEIGHT), *tx, { out }, {}));

  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount());

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeTypeMultisignature | ITransfersContainer::IncludeStateLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeTypeMultisignature | ITransfersContainer::IncludeStateUnlocked));
}

TEST_F(TransfersContainer_addTransaction, handlesAddingConfirmedOutputMultisignature) {
  auto tx = createTransaction();
  auto out = addTestMultisignatureOutput(*tx, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);

  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, { out }, {}));

  container.advanceHeight(1000);

  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount());

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeTypeMultisignature | ITransfersContainer::IncludeStateUnlocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeTypeMultisignature | ITransfersContainer::IncludeStateLocked));
}

TEST_F(TransfersContainer_addTransaction, addingConfirmedOutputMultisignatureTwiceFails) {

  {
    auto tx = createTransaction();
    auto out = addTestMultisignatureOutput(*tx, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
    ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, { out }, {}));
  }

  {
    auto tx = createTransaction();
    auto out = addTestMultisignatureOutput(*tx, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
    ASSERT_ANY_THROW(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT + 1), *tx, { out }, {}));
  }
}


TEST_F(TransfersContainer_addTransaction, ignoresUnrelatedTransactionsWithKeyInput) {
  auto tx = createTransaction();
  addTestInput(*tx, TEST_OUTPUT_AMOUNT);
  ASSERT_FALSE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, {}, {}));
}

TEST_F(TransfersContainer_addTransaction, ignoresUnrelatedTransactionsWithMultisignatureInput) {
  auto tx = createTransaction();
  
  TransactionTypes::InputMultisignature input;
  input.amount = TEST_OUTPUT_AMOUNT;
  input.outputIndex = TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX;
  input.signatures = 1;

  tx->addInput(input);

  ASSERT_FALSE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, {}, {}));
}

TEST_F(TransfersContainer_addTransaction, spendingUnconfirmedOutputFails) {
  auto tx = addTransaction(UNCONFIRMED_TRANSACTION_HEIGHT);

  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount());

  auto outputs = container.getTransactionOutputs(
    tx->getTransactionHash(), ITransfersContainer::IncludeAll);

  ASSERT_EQ(1, outputs.size());

  auto spendingTx = createTransaction();
  for (const auto& t : outputs) {
    addInput(*spendingTx, account, t);
  }

  ASSERT_ANY_THROW(container.addTransaction(blockInfo(UNCONFIRMED_TRANSACTION_HEIGHT), *spendingTx, {}, {}));
}

TEST_F(TransfersContainer_addTransaction, spendingConfirmedOutputWithUnconfirmedTxSucceed) {
  auto tx = addTransaction(TEST_BLOCK_HEIGHT);
  container.advanceHeight(1000);
  auto spendingTx = addSpendingTransaction(tx->getTransactionHash(), 
    UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount()); // no new outputs
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, spendingConfirmedOutputWithConfirmedTxSucceed) {
  auto tx = addTransaction(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  auto spendingTx = addSpendingTransaction(tx->getTransactionHash(),
    TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX + 1);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2);
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount()); // no new outputs
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}


TEST_F(TransfersContainer_addTransaction, spendingConfirmedMultisignatureOutputWithUnconfirmedTxSucceed) {
  auto tx = createTransaction();
  addTestInput(*tx, TEST_OUTPUT_AMOUNT + 1);
  auto out = addTestMultisignatureOutput(*tx, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, { out }, {}));
  
  container.advanceHeight(1000);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  auto spendingTx = createTransaction();
  TransactionTypes::InputMultisignature msigInput{ TEST_OUTPUT_AMOUNT, 2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX };
  spendingTx->addInput(msigInput);

  ASSERT_TRUE(container.addTransaction(blockInfo(UNCONFIRMED_TRANSACTION_HEIGHT), *spendingTx, {}, {}));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, spendingConfirmedMultisignatureOutputWithConfirmedTxSucceed) {
  auto tx = createTransaction();
  addTestInput(*tx, TEST_OUTPUT_AMOUNT + 1);
  auto out = addTestMultisignatureOutput(*tx, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, { out }, {}));

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  auto spendingTx = createTransaction();
  TransactionTypes::InputMultisignature msigInput{ TEST_OUTPUT_AMOUNT, 2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX };
  spendingTx->addInput(msigInput);

  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE), *spendingTx, {}, {}));

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}


//--------------------------------------------------------------------------- 
// TransfersContainer_deleteUnconfirmedTransaction
//--------------------------------------------------------------------------- 
class TransfersContainer_deleteUnconfirmedTransaction : public TransfersContainerTest{};

TEST_F(TransfersContainer_deleteUnconfirmedTransaction, tryDeleteNonExistingTx) {
  addTransaction();
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_FALSE(container.deleteUnconfirmedTransaction(crypto::rand<CryptoNote::Hash>()));
  ASSERT_EQ(1, container.transactionsCount());
}

TEST_F(TransfersContainer_deleteUnconfirmedTransaction, tryDeleteConfirmedTx) {
  auto txHash = addTransaction(TEST_BLOCK_HEIGHT)->getTransactionHash();
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_FALSE(container.deleteUnconfirmedTransaction(txHash));
  ASSERT_EQ(1, container.transactionsCount());
}

TEST_F(TransfersContainer_deleteUnconfirmedTransaction, deleteUnconfirmedSpendingTx) {
  addTransaction(TEST_BLOCK_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);

  ASSERT_EQ(1, transfers.size());

  auto spendingTx = createTransaction();

  {
    CryptoNote::BlockInfo blockInfo{ UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };
    addInput(*spendingTx, account, transfers[0]);
    auto outInfo = addTestKeyOutput(*spendingTx, TEST_OUTPUT_AMOUNT - 1, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
    std::vector<TransactionOutputInformationIn> outputs;
    ASSERT_TRUE(container.addTransaction(blockInfo, *spendingTx, outputs, {}));
  }

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_TRUE(container.deleteUnconfirmedTransaction(spendingTx->getTransactionHash()));

  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_deleteUnconfirmedTransaction, deleteTx) {
  auto txHash = addTransaction()->getTransactionHash();
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_TRUE(container.deleteUnconfirmedTransaction(txHash));
  ASSERT_EQ(0, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
}


//--------------------------------------------------------------------------- 
// TransfersContainer_markTransactionConfirmed
//--------------------------------------------------------------------------- 
class TransfersContainer_markTransactionConfirmed : public TransfersContainerTest {
public:
  bool markConfirmed(const Hash& txHash, uint64_t height = TEST_BLOCK_HEIGHT, 
    const std::vector<uint64_t>& globalIndices = { TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX }) {
    return container.markTransactionConfirmed(blockInfo(height), txHash, globalIndices);
  }
};

TEST_F(TransfersContainer_markTransactionConfirmed, unconfirmedBlockHeight) {
  ASSERT_ANY_THROW(markConfirmed(addTransaction()->getTransactionHash(), UNCONFIRMED_TRANSACTION_HEIGHT));
}

TEST_F(TransfersContainer_markTransactionConfirmed, nonExistingTransaction) {
  addTransaction();
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_FALSE(markConfirmed(crypto::rand<PublicKey>()));
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
}

TEST_F(TransfersContainer_markTransactionConfirmed, confirmedTransaction) {
  auto txHash = addTransaction(TEST_BLOCK_HEIGHT)->getTransactionHash();
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_FALSE(markConfirmed(txHash));
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_markTransactionConfirmed, globalIndicesSmaller) {
  auto tx = createTransaction();

  addTestInput(*tx, TEST_OUTPUT_AMOUNT + 1);

  auto outputs = {
    addTestKeyOutput(*tx, TEST_OUTPUT_AMOUNT / 2, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, account),
    addTestKeyOutput(*tx, TEST_OUTPUT_AMOUNT / 2, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, account)
  };

  ASSERT_TRUE(container.addTransaction(blockInfo(UNCONFIRMED_TRANSACTION_HEIGHT), *tx, outputs, {}));
  ASSERT_EQ(2, container.transfersCount());
  ASSERT_ANY_THROW(markConfirmed(tx->getTransactionHash(), TEST_BLOCK_HEIGHT));
}

TEST_F(TransfersContainer_markTransactionConfirmed, confirmationWorks) {
  auto txHash = addTransaction()->getTransactionHash();
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_TRUE(markConfirmed(txHash));
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_markTransactionConfirmed, confirmationTxWithNoOutputs) {
  addTransaction(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  auto tx = createTransaction();

  {
    addInput(*tx, account, transfers[0]);
    ASSERT_TRUE(container.addTransaction(blockInfo(UNCONFIRMED_TRANSACTION_HEIGHT), *tx, {}, {}));
  }

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_TRUE(markConfirmed(tx->getTransactionHash()));
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_markTransactionConfirmed, confirmingMultisignatureOutputIdenticalAnotherUnspentOuputCausesException) {
  // Add tx1
  CryptoNote::BlockInfo blockInfo1{ TEST_BLOCK_HEIGHT, 1000000 };
  auto tx1 = createTransaction();
  addTestInput(*tx1, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo1 = addTestMultisignatureOutput(*tx1, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs1;
  outputs1.emplace_back(outInfo1);

  ASSERT_TRUE(container.addTransaction(blockInfo1, *tx1, outputs1, {}));

  // Spend output, add tx2
  CryptoNote::BlockInfo blockInfo2{ UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };
  auto tx2 = createTransaction();
  TransactionTypes::InputMultisignature input2;
  input2.amount = TEST_OUTPUT_AMOUNT;
  input2.outputIndex = outInfo1.globalOutputIndex;
  input2.signatures = outInfo1.requiredSignatures;
  tx2->addInput(input2);
  ASSERT_TRUE(container.addTransaction(blockInfo2, *tx2, std::vector<TransactionOutputInformationIn>(), {}));

  // Add tx3
  CryptoNote::BlockInfo blockInfo3{ UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };
  auto tx3 = createTransaction();
  addTestInput(*tx3, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo3 = addTestMultisignatureOutput(*tx3, TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs3;
  outputs3.emplace_back(outInfo3);

  ASSERT_TRUE(container.addTransaction(blockInfo3, *tx3, outputs3, {}));

  // Confirm tx3
  blockInfo3.height = TEST_BLOCK_HEIGHT + 2;
  std::vector<uint64_t> globalIndices3;
  globalIndices3.emplace_back(TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_ANY_THROW(container.markTransactionConfirmed(blockInfo3, tx3->getTransactionHash(), globalIndices3));

  container.advanceHeight(1000);

  ASSERT_EQ(2, container.transfersCount());
  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_markTransactionConfirmed, confirmingMultisignatureOutputIdenticalAnotherSpentOuputCausesException) {
  CryptoNote::BlockInfo blockInfo1{ TEST_BLOCK_HEIGHT, 1000000 };
  auto tx1 = createTransaction();
  addTestInput(*tx1, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo1 = addTestMultisignatureOutput(*tx1, TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs1;
  outputs1.emplace_back(outInfo1);

  ASSERT_TRUE(container.addTransaction(blockInfo1, *tx1, outputs1, {}));

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  CryptoNote::BlockInfo blockInfo2{ UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };
  auto tx2 = createTransaction();
  addTestInput(*tx2, TEST_OUTPUT_AMOUNT + 1);
  auto outInfo2 = addTestMultisignatureOutput(*tx2, TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs2;
  outputs2.emplace_back(outInfo2);

  ASSERT_TRUE(container.addTransaction(blockInfo2, *tx2, outputs2, {}));

  blockInfo2.height = TEST_BLOCK_HEIGHT + 2;
  std::vector<uint64_t> globalIndices2;
  globalIndices2.emplace_back(TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_ANY_THROW(container.markTransactionConfirmed(blockInfo2, tx2->getTransactionHash(), globalIndices2));

  ASSERT_EQ(2, container.transfersCount());
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

//--------------------------------------------------------------------------- 
// TransfersContainer_detach
//--------------------------------------------------------------------------- 
class TransfersContainer_detach : public TransfersContainerTest {
public:
  
};

TEST_F(TransfersContainer_detach, detachConfirmed) {
  addTransaction(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());

  detachContainer(TEST_BLOCK_HEIGHT);

  ASSERT_EQ(0, container.transfersCount());
  ASSERT_EQ(0, container.transactionsCount());
}

TEST_F(TransfersContainer_detach, detachConfirmedSpendingTransaction) {
  auto tx = addTransaction(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  auto spendingTx = addSpendingTransaction(
    tx->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  detachContainer(TEST_BLOCK_HEIGHT+1);
  
  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_detach, threeRelatedTransactions) {
  auto tx = addTransaction(TEST_BLOCK_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  auto spendingTx1 = addSpendingTransaction(
    tx->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT / 2);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2);

  auto spendingTx2 = addSpendingTransaction(
    spendingTx1->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX + 2, TEST_OUTPUT_AMOUNT / 2);

  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  detachContainer(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_detach, detachConfirmedTransactionWithUnrelatedUnconfirmed) {
  auto tx1 = addTransaction(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransaction(UNCONFIRMED_TRANSACTION_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, container.balance(ITransfersContainer::IncludeAll));

  detachContainer(TEST_BLOCK_HEIGHT);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_detach, confirmedWithUnconfirmedSpendingTransaction_H1) {

  auto tx = addTransaction(TEST_BLOCK_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  auto spendingTx = addSpendingTransaction(
    tx->getTransactionHash(), UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  detachContainer(TEST_BLOCK_HEIGHT + 1);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_detach, confirmedWithUnconfirmedSpendingTransaction_H0) {
  auto tx = addTransaction(TEST_BLOCK_HEIGHT);
  
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  auto spendingTx = addSpendingTransaction(
    tx->getTransactionHash(), UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  detachContainer(TEST_BLOCK_HEIGHT);

  ASSERT_EQ(0, container.transfersCount());
  ASSERT_EQ(0, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_detach, confirmedTwoOfThree) {
  auto txHash = addTransaction(TEST_BLOCK_HEIGHT - 1)->getTransactionHash();
  addTransaction(TEST_BLOCK_HEIGHT);
  addTransaction(TEST_BLOCK_HEIGHT + 1);

  ASSERT_EQ(3, container.transactionsCount());

  detachContainer(TEST_BLOCK_HEIGHT);

  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(1, container.getTransactionOutputs(txHash, ITransfersContainer::IncludeAll).size());
}

TEST_F(TransfersContainer_detach, transactionDetachAfterAdvance) {
  addTransaction(TEST_BLOCK_HEIGHT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  detachContainer(TEST_BLOCK_HEIGHT);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

//--------------------------------------------------------------------------- 
// TransfersContainer_advanceHeight
//--------------------------------------------------------------------------- 
class TransfersContainer_advanceHeight : public TransfersContainerTest {
public:
  TransfersContainer_advanceHeight(){}
};

TEST_F(TransfersContainer_advanceHeight, advanceUnlocksTransaction) {
  addTransaction(TEST_BLOCK_HEIGHT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  addTransaction(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

//---------------------------------------------------------------------------
// TransfersContainer_transfersLockStateNotification
//---------------------------------------------------------------------------
class TransfersContainer_transfersLockStateNotification : public TransfersContainer_advanceHeight {
public:
  TransfersContainer_transfersLockStateNotification(){}

  const uint64_t AMOUNT_1 = 1122;
  const uint64_t AMOUNT_2 = 2233;
  const uint32_t TERM = 100;
  const uint64_t TRANSACTION_HEIGHT_1 = 1;

  struct UnconfirmedTransactionInfo {
    std::unique_ptr<ITransaction> transaction;
    std::vector<TransactionOutputInformationIn> outputs;
    std::vector<uint64_t> globalIndices;

    UnconfirmedTransactionInfo() {
    }

    UnconfirmedTransactionInfo(UnconfirmedTransactionInfo&& other) :
      transaction(std::move(other.transaction)),
      outputs(std::move(other.outputs)),
      globalIndices(std::move(other.globalIndices)) {
    }
  };

  UnconfirmedTransactionInfo addUnconfirmedTransaction(uint64_t outputAmount = TEST_OUTPUT_AMOUNT) {
    UnconfirmedTransactionInfo txInfo;

    txInfo.transaction = createTransaction();

    addTestInput(*txInfo.transaction, outputAmount + 1);
    auto outputIndex = UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX;
    auto outInfo = addTestKeyOutput(*txInfo.transaction, outputAmount, outputIndex, account);

    txInfo.outputs = { outInfo };
    txInfo.globalIndices = { TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX };

    std::vector<TransactionOutputInformation> unlockedTransfers;
    EXPECT_TRUE(container.addTransaction(blockInfo(UNCONFIRMED_TRANSACTION_HEIGHT), *txInfo.transaction, txInfo.outputs, {}, &unlockedTransfers));
    EXPECT_TRUE(unlockedTransfers.empty());

    return txInfo;
  }

  void confirmTransaction(uint64_t height, const UnconfirmedTransactionInfo& txInfo) {
    EXPECT_TRUE(container.markTransactionConfirmed(blockInfo(height), txInfo.transaction->getTransactionHash(), txInfo.globalIndices));
  }

  std::unique_ptr<ITransaction> addDepositTransaction(uint64_t height, uint32_t term, uint64_t amount = TEST_OUTPUT_AMOUNT, std::vector<TransactionOutputInformation>* unlockedTransfers = nullptr) {
    auto tx = createTransaction();
    auto outInfo = addDepositOutput(*tx, amount, term, height);

    std::vector<TransactionOutputInformationIn> outputs = { outInfo };

    EXPECT_TRUE(container.addTransaction(blockInfo(height), *tx, outputs, {}, unlockedTransfers));

    return tx;
  }
};

TEST_F(TransfersContainer_transfersLockStateNotification, addTransactionReturnsUnlockedTransfers) {
  std::vector<CryptoNote::TransactionOutputInformation> unlockedTransfers;

  addTransaction(TRANSACTION_HEIGHT_1, AMOUNT_1);
  addTransaction(TRANSACTION_HEIGHT_1 + TEST_TRANSACTION_SPENDABLE_AGE, AMOUNT_2, &unlockedTransfers);

  ASSERT_EQ(1, unlockedTransfers.size());
  EXPECT_EQ(AMOUNT_1, unlockedTransfers.front().amount);
}

TEST_F(TransfersContainer_transfersLockStateNotification, advanceReturnsUnlockedTransfers) {
  std::vector<CryptoNote::TransactionOutputInformation> unlockedTransfers;

  addTransaction(TRANSACTION_HEIGHT_1, AMOUNT_1);
  unlockedTransfers = container.advanceHeight(TRANSACTION_HEIGHT_1 + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_EQ(1, unlockedTransfers.size());
  EXPECT_EQ(AMOUNT_1, unlockedTransfers.front().amount);
}

TEST_F(TransfersContainer_transfersLockStateNotification, advanceReturnsNoUnlockedTransfersForUnconfirmedTransaction) {
  auto info = addUnconfirmedTransaction(TEST_OUTPUT_AMOUNT);

  auto unlockedTransfers = container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_TRUE(unlockedTransfers.empty());
}

TEST_F(TransfersContainer_transfersLockStateNotification, advanceReturnsUnlockedTransfersAfterTransactionConfirmation) {
  auto info = addUnconfirmedTransaction(TEST_OUTPUT_AMOUNT);
  confirmTransaction(TEST_BLOCK_HEIGHT, info);

  auto unlockedTransfers = container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_EQ(1, unlockedTransfers.size());
  EXPECT_EQ(TEST_OUTPUT_AMOUNT, unlockedTransfers[0].amount);
}

TEST_F(TransfersContainer_transfersLockStateNotification, secondAdvanceReturnsNoUnlockedTransfers) {
  addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_1);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  auto transfers = container.advanceHeight(TEST_BLOCK_HEIGHT + 2 * TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_TRUE(transfers.empty());
}

TEST_F(TransfersContainer_transfersLockStateNotification, advanceHeightLessThenDepositTermReturnsNoTransfers) {
  assert(TEST_TRANSACTION_SPENDABLE_AGE < TERM);
  assert(TERM > 1);

  addDepositTransaction(TEST_BLOCK_HEIGHT, TERM);

  auto unlocked = container.advanceHeight(TEST_BLOCK_HEIGHT + TERM - 1);
  ASSERT_TRUE(unlocked.empty());
}

TEST_F(TransfersContainer_transfersLockStateNotification, advanceHeightReturnsUnlockedDeposits) {
  addDepositTransaction(TEST_BLOCK_HEIGHT, TERM, TEST_OUTPUT_AMOUNT);

  auto unlocked = container.advanceHeight(TEST_BLOCK_HEIGHT + TERM);
  ASSERT_EQ(1, unlocked.size());
  EXPECT_EQ(TEST_OUTPUT_AMOUNT, unlocked[0].amount);
}

TEST_F(TransfersContainer_transfersLockStateNotification, detachLocksTransfers) {
  addTransaction(TEST_BLOCK_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  std::vector<TransactionOutputInformation> lockedTransfers = detachContainerGetLockedTransfers(TEST_BLOCK_HEIGHT + 1);

  ASSERT_EQ(1, lockedTransfers.size());
  EXPECT_EQ(TEST_OUTPUT_AMOUNT, lockedTransfers[0].amount);
}

TEST_F(TransfersContainer_transfersLockStateNotification, detachDoesntLockUnconfirmedTransfers) {
  addTransaction();

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  std::vector<TransactionOutputInformation> lockedTransfers = detachContainerGetLockedTransfers(TEST_BLOCK_HEIGHT + 1);
  ASSERT_TRUE(lockedTransfers.empty());
}

TEST_F(TransfersContainer_transfersLockStateNotification, detachDoesntLockUnlockedTransfersForThisHeight) {
  addTransaction(TEST_BLOCK_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 10);

  std::vector<TransactionOutputInformation> lockedTransfers = detachContainerGetLockedTransfers(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1);
  ASSERT_TRUE(lockedTransfers.empty());
}

TEST_F(TransfersContainer_transfersLockStateNotification, detachLocksDeposits) {
  assert(TEST_TRANSACTION_SPENDABLE_AGE <= TERM);

  addDepositTransaction(TEST_BLOCK_HEIGHT, TERM, TEST_OUTPUT_AMOUNT);

  uint64_t unlockHeight = TEST_BLOCK_HEIGHT + TERM;
  container.advanceHeight(unlockHeight);

  std::vector<TransactionOutputInformation> lockedTransfers = detachContainerGetLockedTransfers(unlockHeight - 1);
  ASSERT_EQ(1, lockedTransfers.size());
  EXPECT_EQ(TEST_OUTPUT_AMOUNT, lockedTransfers[0].amount);
}

TEST_F(TransfersContainer_transfersLockStateNotification, advanceHeightDoesntUnlockDeletedTransfers) {
  addTransaction(TEST_BLOCK_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  detachContainer(TEST_BLOCK_HEIGHT);
  auto unlocked = container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_TRUE(unlocked.empty());
}

TEST_F(TransfersContainer_transfersLockStateNotification, restoreContainerAndUnlockConfirmedTransactions) {
  addTransaction(TEST_BLOCK_HEIGHT);

  std::stringstream stream;
  container.save(stream);
  TransfersContainer container2(currency, TEST_TRANSACTION_SPENDABLE_AGE);
  container2.load(stream);

  auto unlocked = container2.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(1, unlocked.size());
  EXPECT_EQ(TEST_OUTPUT_AMOUNT, unlocked[0].amount);
}

TEST_F(TransfersContainer_transfersLockStateNotification, restoreContainerAndUnlockUnconfirmedTransactions) {
  addTransaction();

  std::stringstream stream;
  container.save(stream);
  TransfersContainer container2(currency, TEST_TRANSACTION_SPENDABLE_AGE);
  container2.load(stream);

  auto unlocked = container2.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_TRUE(unlocked.empty());
}

//--------------------------------------------------------------------------- 
// TransfersContainer_balance
//--------------------------------------------------------------------------- 

class TransfersContainer_balance : public TransfersContainerTest {
public:
  TransfersContainer_balance() {
  }

  enum TestAmounts : uint64_t {
    AMOUNT_1 = 13,
    AMOUNT_2 = 17
  };
};


TEST_F(TransfersContainer_balance, treatsUnconfirmedTransfersAsLocked) {
  auto tx1 = addTransaction(UNCONFIRMED_TRANSACTION_HEIGHT, AMOUNT_1);
  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainer_balance, handlesLockedByTimeTransferAsLocked) {
  auto tx1 = createTransaction();
  tx1->setUnlockTime(time(nullptr) + 60 * 60 * 24);
  addTestInput(*tx1, AMOUNT_1 + 1);
  auto outInfo = addTestKeyOutput(*tx1, AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo };
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx1, outputs, {}));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainer_balance, handlesLockedByHeightTransferAsLocked) {
  auto tx1 = createTransaction();
  tx1->setUnlockTime(TEST_CONTAINER_CURRENT_HEIGHT + 1);
  addTestInput(*tx1, AMOUNT_1 + 1);
  auto outInfo = addTestKeyOutput(*tx1, AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo };
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx1, outputs, {}));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainer_balance, handlesTransferStateSoftLocked) {
  auto tx1 = addTransaction(TEST_CONTAINER_CURRENT_HEIGHT - TEST_TRANSACTION_SPENDABLE_AGE, AMOUNT_2);
  auto tx2 = addTransaction(TEST_CONTAINER_CURRENT_HEIGHT, AMOUNT_1);

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainer_balance, handlesTransferStateUnLocked) {
  auto tx1 = addTransaction(TEST_CONTAINER_CURRENT_HEIGHT - TEST_TRANSACTION_SPENDABLE_AGE, AMOUNT_2);
  auto tx2 = addTransaction(TEST_CONTAINER_CURRENT_HEIGHT, AMOUNT_1);

  ASSERT_EQ(AMOUNT_2, container.balance(ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainer_balance, handlesTransferTypeKey) {
  auto tx = createTransaction();
  addTestInput(*tx, AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = addTestKeyOutput(*tx, AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  auto outInfo2 = addTestMultisignatureOutput(*tx, AMOUNT_2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo1, outInfo2 };
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, outputs, {}));

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateAll | ITransfersContainer::IncludeTypeKey));
}

TEST_F(TransfersContainer_balance, handlesTransferTypeMultisignature) {
  auto tx = createTransaction();
  addTestInput(*tx, AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = addTestKeyOutput(*tx, AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  auto outInfo2 = addTestMultisignatureOutput(*tx, AMOUNT_2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo1, outInfo2 };
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, outputs, {}));

  ASSERT_EQ(AMOUNT_2, container.balance(ITransfersContainer::IncludeStateAll | ITransfersContainer::IncludeTypeMultisignature));
}

TEST_F(TransfersContainer_balance, filtersByStateAndKeySimultaneously) {
  auto tx1 = createTransaction();
  addTestInput(*tx1, AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = addTestKeyOutput(*tx1, AMOUNT_1, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, account);
  auto outInfo2 = addTestMultisignatureOutput(*tx1, AMOUNT_2, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo1, outInfo2 };
  ASSERT_TRUE(container.addTransaction(blockInfo(UNCONFIRMED_TRANSACTION_HEIGHT), *tx1, outputs, {}));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_1 + AMOUNT_2);

  container.advanceHeight(TEST_CONTAINER_CURRENT_HEIGHT);

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeKey));
  ASSERT_EQ(AMOUNT_2, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeMultisignature));
  ASSERT_EQ(AMOUNT_1 + AMOUNT_2, container.balance(ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeTypeKey));
}


//--------------------------------------------------------------------------- 
// TransfersContainer_getOutputs
//--------------------------------------------------------------------------- 

class TransfersContainer_getOutputs : public TransfersContainerTest {
public:
  TransfersContainer_getOutputs() {
  }

  enum TestAmounts : uint64_t {
    AMOUNT_1 = 13,
    AMOUNT_2 = 17
  };
};


TEST_F(TransfersContainer_getOutputs, treatsUnconfirmedTransfersAsLocked) {
  auto tx1 = addTransaction(UNCONFIRMED_TRANSACTION_HEIGHT, AMOUNT_1);
  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesLockedByTimeTransferAsLocked) {
  auto tx1 = createTransaction();
  tx1->setUnlockTime(time(nullptr) + 60 * 60 * 24);
  addTestInput(*tx1, AMOUNT_1 + 1);
  auto outInfo = addTestKeyOutput(*tx1, AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo };
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx1, outputs, {}));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesLockedByHeightTransferAsLocked) {
  auto tx1 = createTransaction();
  tx1->setUnlockTime(TEST_CONTAINER_CURRENT_HEIGHT + 1);
  addTestInput(*tx1, AMOUNT_1 + 1);
  auto outInfo = addTestKeyOutput(*tx1, AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo };
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx1, outputs, {}));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesTransferStateSoftLocked) {
  addTransaction(TEST_CONTAINER_CURRENT_HEIGHT - TEST_TRANSACTION_SPENDABLE_AGE, AMOUNT_2);
  addTransaction(TEST_CONTAINER_CURRENT_HEIGHT, AMOUNT_1);
  
  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesTransferStateUnLocked) {
  addTransaction(TEST_CONTAINER_CURRENT_HEIGHT - TEST_TRANSACTION_SPENDABLE_AGE, AMOUNT_2);
  addTransaction(TEST_CONTAINER_CURRENT_HEIGHT, AMOUNT_1);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeTypeAll);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_2, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesTransferTypeKey) {
  auto tx = createTransaction();
  addTestInput(*tx, AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = addTestKeyOutput(*tx, AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  auto outInfo2 = addTestMultisignatureOutput(*tx, AMOUNT_2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo1, outInfo2 };
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, outputs, {}));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateAll | ITransfersContainer::IncludeTypeKey);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesTransferTypeMultisignature) {
  auto tx = createTransaction();
  addTestInput(*tx, AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = addTestKeyOutput(*tx, AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  auto outInfo2 = addTestMultisignatureOutput(*tx, AMOUNT_2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo1, outInfo2 };
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, outputs, {}));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateAll | ITransfersContainer::IncludeTypeMultisignature);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_2, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, filtersByStateAndKeySimultaneously) {
  auto tx1 = createTransaction();
  addTestInput(*tx1, AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = addTestKeyOutput(*tx1, AMOUNT_1, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, account);
  auto outInfo2 = addTestMultisignatureOutput(*tx1, AMOUNT_2, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo1, outInfo2 };
  ASSERT_TRUE(container.addTransaction(blockInfo(UNCONFIRMED_TRANSACTION_HEIGHT), *tx1, outputs, {}));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_1 + AMOUNT_2);

  container.advanceHeight(TEST_CONTAINER_CURRENT_HEIGHT);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeKey);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);

  transfers.clear();
  container.getOutputs(transfers, ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeMultisignature);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_2, transfers.front().amount);

  transfers.clear();
  container.getOutputs(transfers, ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeTypeKey);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1 + AMOUNT_2, transfers.front().amount);
}

class TransfersContainer_getTransactionInputs : public TransfersContainerTest {
public:
  const uint64_t AMOUNT_1 = 1000224;
  const uint64_t AMOUNT_2 = 3392922;
  const uint64_t AMOUNT_3 = AMOUNT_1 + AMOUNT_2;
  const uint64_t TERM = 22231;

  TransfersContainer_getTransactionInputs() {
  }

  std::vector<TransactionOutputInformationIn> keyOutputs;
  std::vector<TransactionOutputInformationIn> depositOutputs;

  std::unique_ptr<ITransaction> createGeneratingTransaction() {
    auto generatingTransaction = createTransaction();

    auto out1 = addTestKeyOutput(*generatingTransaction, AMOUNT_1, 1, account);
    auto out2 = addTestKeyOutput(*generatingTransaction, AMOUNT_2, 2, account);
    auto out3 = addDepositOutput(*generatingTransaction, AMOUNT_3, TERM, TEST_BLOCK_HEIGHT);

    std::vector<TransactionOutputInformationIn> outputs = { out1, out2, out3 };
    container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *generatingTransaction, outputs, {});
    container.advanceHeight(TEST_BLOCK_HEIGHT + TERM);

    out1.transactionHash = generatingTransaction->getTransactionHash();
    out2.transactionHash = generatingTransaction->getTransactionHash();

    keyOutputs.push_back(out1);
    keyOutputs.push_back(out2);
    depositOutputs.push_back(out3);

    return generatingTransaction;
  }

  std::unique_ptr<ITransaction> createSpendingTransaction(ITransaction& generatingTransaction) {
    auto spendingTransaction = addSpendingTransaction(generatingTransaction.getTransactionHash(), TEST_BLOCK_HEIGHT + TERM + 1, 3, AMOUNT_3);
    return spendingTransaction;
  }

  void checkKeyInputs(const std::vector<TransactionOutputInformation>& inputs) {
    for (const auto& input: keyOutputs) {
      EXPECT_NE(inputs.end(), std::find(inputs.begin(), inputs.end(), static_cast<const TransactionOutputInformation&>(input)));
    }
  }

  void checkDepositInputs(const std::vector<TransactionOutputInformation>& inputs) {
    for (const auto& input: depositOutputs) {
      EXPECT_NE(inputs.end(), std::find(inputs.begin(), inputs.end(), static_cast<const TransactionOutputInformation&>(input)));
    }
  }

  void checkDepositInputs(ITransaction& spendingTransaction) {
    std::vector<TransactionOutputInformation> inputs = container.getTransactionInputs(spendingTransaction.getTransactionHash(), ITransfersContainer::IncludeTypeDeposit);
    ASSERT_EQ(depositOutputs.size(), inputs.size());

    checkDepositInputs(inputs);
  }

  void checkKeyInputs(ITransaction& spendingTransaction) {
    std::vector<TransactionOutputInformation> inputs = container.getTransactionInputs(spendingTransaction.getTransactionHash(), ITransfersContainer::IncludeTypeKey);
    ASSERT_EQ(keyOutputs.size(), inputs.size());

    checkKeyInputs(inputs);
  }

  void checkAllInputs(ITransaction& spendingTransaction) {
    std::vector<TransactionOutputInformation> inputs = container.getTransactionInputs(spendingTransaction.getTransactionHash(), ITransfersContainer::IncludeTypeAll);

    ASSERT_EQ(keyOutputs.size() + depositOutputs.size(), inputs.size());
    checkKeyInputs(inputs);
    checkDepositInputs(inputs);
  }
};

TEST_F(TransfersContainer_getTransactionInputs, returnsAllInputsCorrectly) {
  auto generatingTransaction = createGeneratingTransaction();
  auto spendingTransaction = createSpendingTransaction(*generatingTransaction);

  checkAllInputs(*spendingTransaction);
}

TEST_F(TransfersContainer_getTransactionInputs, returnsDepositInputsCorrectly) {
  auto generatingTransaction = createGeneratingTransaction();
  auto spendingTransaction = createSpendingTransaction(*generatingTransaction);

  checkDepositInputs(*spendingTransaction);
}

TEST_F(TransfersContainer_getTransactionInputs, returnsKeyInputsCorrectly) {
  auto generatingTransaction = createGeneratingTransaction();
  auto spendingTransaction = createSpendingTransaction(*generatingTransaction);

  checkKeyInputs(*spendingTransaction);
}

class TransfersContainer_depositBalance : public TransfersContainer_getTransactionInputs {
protected:

  const uint64_t HEIGHT = 10;
  const uint64_t TERM = 100;

  void addTransaction(const std::unique_ptr<ITransaction>& tx, TransactionOutputInformationIn outInfo, uint64_t height) {
    std::vector<TransactionOutputInformationIn> outputs = { outInfo };
    EXPECT_TRUE(container.addTransaction(blockInfo(height), *tx, outputs, {}));
  }

  void addTransaction(const std::unique_ptr<ITransaction>& tx, uint64_t height) {
    EXPECT_TRUE(container.addTransaction(blockInfo(height), *tx, {}, {}));
  }
};

TEST_F(TransfersContainer_depositBalance, depositBalanceLocked) {
  auto tx = createTransaction();

  TransactionOutputInformationIn outInfo = addDepositOutput(*tx, TEST_OUTPUT_AMOUNT, TERM, HEIGHT);
  addTransaction(tx, outInfo, HEIGHT);
  container.advanceHeight(HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  EXPECT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  EXPECT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeTypeAll | ITransfersContainer::IncludeStateLocked));
}

TEST_F(TransfersContainer_depositBalance, depositBalanceUnlocked) {
  auto tx = createTransaction();

  TransactionOutputInformationIn outInfo = addDepositOutput(*tx, TEST_OUTPUT_AMOUNT, TERM, HEIGHT);
  addTransaction(tx, outInfo, HEIGHT);
  container.advanceHeight(HEIGHT + TERM);

  EXPECT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  EXPECT_EQ(0, container.balance(ITransfersContainer::IncludeTypeAll | ITransfersContainer::IncludeStateLocked));
}

TEST_F(TransfersContainer_depositBalance, spendDepositOutput) {
  auto tx = createTransaction();

  TransactionOutputInformationIn outInfo = addDepositOutput(*tx, TEST_OUTPUT_AMOUNT, TERM, HEIGHT);
  addTransaction(tx, outInfo, HEIGHT);
  container.advanceHeight(HEIGHT + TERM);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);

  ASSERT_EQ(1, transfers.size());

  auto spendingTx = createTransaction();
  TransactionTypes::InputMultisignature depositInput;
  depositInput.amount = transfers[0].amount;
  depositInput.outputIndex = transfers[0].globalOutputIndex;
  depositInput.term = transfers[0].term;
  depositInput.signatures = transfers[0].requiredSignatures;
  spendingTx->addInput(depositInput);

  addTransaction(spendingTx, HEIGHT + TERM + 1);
  EXPECT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_depositBalance, serializeContainer) {
  auto tx1 = createTransaction();

  TransactionOutputInformationIn out1 = addDepositOutput(*tx1, TEST_OUTPUT_AMOUNT, TERM, HEIGHT);
  addTransaction(tx1, out1, HEIGHT);
  container.advanceHeight(HEIGHT + TERM);

  const uint64_t HEIGHT2 = HEIGHT + TERM;
  const uint64_t TERM2 = 37;
  const uint64_t AMOUNT2 = 8832;

  auto tx2 = createTransaction();
  TransactionOutputInformationIn out2 = addDepositOutput(*tx2, AMOUNT2, TERM2, HEIGHT2);
  addTransaction(tx2, out2, HEIGHT2);

  auto unlockedBalance = container.balance(ITransfersContainer::IncludeAllUnlocked);
  auto lockedBalance = container.balance(ITransfersContainer::IncludeAllLocked);

  std::stringstream stream;
  container.save(stream);
  TransfersContainer container2(currency, TEST_TRANSACTION_SPENDABLE_AGE);
  container2.load(stream);

  std::vector<TransactionOutputInformation> transfers;
  container2.getOutputs(transfers, ITransfersContainer::IncludeAll);

  ASSERT_EQ(2, transfers.size());
  EXPECT_EQ(unlockedBalance, container2.balance(ITransfersContainer::IncludeAllUnlocked));
  EXPECT_EQ(lockedBalance, container2.balance(ITransfersContainer::IncludeAllLocked));

  EXPECT_EQ(static_cast<TransactionOutputInformation&>(out1), transfers[0]);
  EXPECT_EQ(static_cast<TransactionOutputInformation&>(out2), transfers[1]);
}

TEST_F(TransfersContainer_depositBalance, transactionUnlockTimeGreaterThenDepositTerm) {
  const uint64_t UNLOCK_TIME = TERM + 20;
  auto tx = createTransaction();

  TransactionOutputInformationIn out = addDepositOutput(*tx, TEST_OUTPUT_AMOUNT, TERM, HEIGHT);
  tx->setUnlockTime(UNLOCK_TIME);

  addTransaction(tx, out, HEIGHT);
  container.advanceHeight(HEIGHT + TERM);

  EXPECT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  EXPECT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeTypeAll | ITransfersContainer::IncludeStateLocked));
}
