// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// epee
#include "misc_language.h"

#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_format_utils.h"

#include "WalletTransactionSender.h"
#include "WalletUtils.h"

#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/TransactionApi.h"

#include <random>

namespace {

using namespace CryptoNote;

PublicKey convertPublicKey(const crypto::public_key& publicKey) {
  PublicKey result = *reinterpret_cast<const PublicKey *>(&publicKey);

  return result;
}

SecretKey convertSecretKey(const crypto::secret_key& secretKey) {
  SecretKey result = *reinterpret_cast<const SecretKey *>(&secretKey);

  return result;
}

AccountAddress convertAccountAddress(const cryptonote::AccountPublicAddress& address) {
  AccountAddress result;
  result.spendPublicKey = convertPublicKey(address.m_spendPublicKey);
  result.viewPublicKey = convertPublicKey(address.m_viewPublicKey);

  return result;
}

AccountKeys convertAccountKeys(const cryptonote::account_keys& keys) {
  AccountKeys result;
  result.address = convertAccountAddress(keys.m_account_address);
  result.spendSecretKey = convertSecretKey(keys.m_spend_secret_key);
  result.viewSecretKey = convertSecretKey(keys.m_view_secret_key);

  return result;
}

uint64_t countNeededMoney(uint64_t fee, const std::vector<CryptoNote::Transfer>& transfers) {
  uint64_t needed_money = fee;
  for (auto& transfer: transfers) {
    CryptoNote::throwIf(transfer.amount == 0, cryptonote::error::ZERO_DESTINATION);
    CryptoNote::throwIf(transfer.amount < 0, cryptonote::error::WRONG_AMOUNT);

    needed_money += transfer.amount;
    CryptoNote::throwIf(static_cast<int64_t>(needed_money) < transfer.amount, cryptonote::error::SUM_OVERFLOW);
  }

  return needed_money;
}

uint64_t getSumWithOverflowCheck(uint64_t amount, uint64_t fee) {
  CryptoNote::throwIf(std::numeric_limits<uint64_t>::max() - amount < fee, cryptonote::error::SUM_OVERFLOW);

  return amount + fee;
}

void createChangeDestinations(const cryptonote::AccountPublicAddress& address, uint64_t neededMoney, uint64_t foundMoney, cryptonote::tx_destination_entry& changeDts) {
  if (neededMoney < foundMoney) {
    changeDts.addr = address;
    changeDts.amount = foundMoney - neededMoney;
  }
}

void constructTx(const cryptonote::account_keys keys, const std::vector<cryptonote::tx_source_entry>& sources, const std::vector<cryptonote::tx_destination_entry>& splittedDests,
    const std::string& extra, uint64_t unlockTimestamp, uint64_t sizeLimit, cryptonote::Transaction& tx, const std::vector<cryptonote::tx_message_entry>& messages) {
  std::vector<uint8_t> extraVec;
  extraVec.reserve(extra.size());
  std::for_each(extra.begin(), extra.end(), [&extraVec] (const char el) { extraVec.push_back(el);});

  bool r = cryptonote::construct_tx(keys, sources, splittedDests, messages, extraVec, unlockTimestamp, tx);
  CryptoNote::throwIf(!r, cryptonote::error::INTERNAL_WALLET_ERROR);
  CryptoNote::throwIf(cryptonote::get_object_blobsize(tx) >= sizeLimit, cryptonote::error::TRANSACTION_SIZE_TOO_BIG);
}

void fillTransactionHash(const cryptonote::Transaction& tx, CryptoNote::TransactionHash& hash) {
  crypto::hash h = cryptonote::get_transaction_hash(tx);
  memcpy(hash.data(), reinterpret_cast<const uint8_t *>(&h), hash.size());
}

std::unique_ptr<WalletEvent> makeCompleteEvent(WalletUserTransactionsCache& transactionCache, size_t transactionId, std::error_code ec) {
  transactionCache.updateTransactionSendingState(transactionId, ec);
  return std::unique_ptr<WalletSendTransactionCompletedEvent>(new WalletSendTransactionCompletedEvent(transactionId, ec));
}

std::vector<TransactionTypes::InputKeyInfo> convertSources(std::vector<cryptonote::tx_source_entry>&& sources) {
  std::vector<TransactionTypes::InputKeyInfo> inputs;
  inputs.reserve(sources.size());

  for (cryptonote::tx_source_entry& source: sources) {
    TransactionTypes::InputKeyInfo input;
    input.amount = source.amount;

    input.outputs.reserve(source.outputs.size());
    for (const cryptonote::tx_source_entry::output_entry& sourceOutput: source.outputs) {
      TransactionTypes::GlobalOutput output;
      output.outputIndex = sourceOutput.first;
      output.targetKey = convertPublicKey(sourceOutput.second);

      input.outputs.emplace_back(std::move(output));
    }

    input.realOutput.transactionPublicKey = convertPublicKey(source.real_out_tx_key);
    input.realOutput.outputInTransaction = source.real_output_in_tx_index;
    input.realOutput.transactionIndex = source.real_output;

    inputs.emplace_back(std::move(input));
  }

  return inputs;
}

std::vector<uint64_t> splitAmount(uint64_t amount, uint64_t dustThreshold) {
  std::vector<uint64_t> amounts;

  cryptonote::decompose_amount_into_digits(amount, dustThreshold,
    [&](uint64_t chunk) { amounts.push_back(chunk); },
    [&](uint64_t dust) { amounts.push_back(dust); } );

  return amounts;
}

cryptonote::Transaction convertTransaction(const ITransaction& transaction) {
  Blob serializedTransaction = transaction.getTransactionData();

  cryptonote::Transaction result;
  if (!parse_and_validate_tx_from_blob(std::string(serializedTransaction.begin(), serializedTransaction.end()), result)) {
    throw std::system_error(make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR), "Cannot convert transaction");
  }

  return result;
}

uint64_t checkDepositsAndCalculateAmount(const std::vector<DepositId>& depositIds, const WalletUserTransactionsCache& transactionsCache) {
  uint64_t amount = 0;

  for (const auto& id: depositIds) {
    Deposit deposit;
    throwIf(!transactionsCache.getDeposit(id, deposit), cryptonote::error::DEPOSIT_DOESNOT_EXIST);
    throwIf(deposit.locked, cryptonote::error::DEPOSIT_LOCKED);

    amount += deposit.amount + deposit.interest;
  }

  return amount;
}

void countDepositsTotalSumAndInterestSum(const std::vector<DepositId>& depositIds, WalletUserTransactionsCache& depositsCache,
                                         uint64_t& totalSum, uint64_t& interestsSum) {
  totalSum = 0;
  interestsSum = 0;

  for (auto id: depositIds) {
    Deposit& deposit = depositsCache.getDeposit(id);
    totalSum += deposit.amount + deposit.interest;
    interestsSum += deposit.interest;
  }
}

} //namespace

namespace CryptoNote {

WalletTransactionSender::WalletTransactionSender(const cryptonote::Currency& currency, WalletUserTransactionsCache& transactionsCache, cryptonote::account_keys keys, ITransfersContainer& transfersContainer) :
  m_currency(currency),
  m_transactionsCache(transactionsCache),
  m_isStoping(false),
  m_keys(keys),
  m_transferDetails(transfersContainer),
  m_upperTransactionSizeLimit(m_currency.blockGrantedFullRewardZone() * 125 / 100 - m_currency.minerTxBlobReservedSize()) {}

void WalletTransactionSender::stop() {
  m_isStoping = true;
}

bool WalletTransactionSender::validateDestinationAddress(const std::string& address) {
  cryptonote::AccountPublicAddress ignore;
  return m_currency.parseAccountAddressString(address, ignore);
}

void WalletTransactionSender::validateTransfersAddresses(const std::vector<Transfer>& transfers) {
  for (const Transfer& tr: transfers) {
    if (!validateDestinationAddress(tr.address)) {
      throw std::system_error(make_error_code(cryptonote::error::BAD_ADDRESS));
    }
  }
}

std::unique_ptr<WalletRequest> WalletTransactionSender::makeSendRequest(TransactionId& transactionId, std::deque<std::unique_ptr<WalletEvent>>& events,
    const std::vector<Transfer>& transfers, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp, const std::vector<TransactionMessage>& messages) {

  using namespace cryptonote;

  throwIf(transfers.empty(), cryptonote::error::ZERO_DESTINATION);
  validateTransfersAddresses(transfers);
  uint64_t neededMoney = countNeededMoney(fee, transfers);

  std::shared_ptr<SendTransactionContext> context = std::make_shared<SendTransactionContext>();
  context->dustPolicy.dustThreshold = m_currency.defaultDustThreshold();

  context->foundMoney = selectTransfersToSend(neededMoney, 0 == mixIn, context->dustPolicy.dustThreshold, context->selectedTransfers);
  throwIf(context->foundMoney < neededMoney, cryptonote::error::WRONG_AMOUNT);

  transactionId = m_transactionsCache.addNewTransaction(neededMoney, fee, extra, transfers, unlockTimestamp, messages);
  context->transactionId = transactionId;
  context->mixIn = mixIn;

  for (const TransactionMessage& message : messages) {
    cryptonote::AccountPublicAddress address;
    bool extracted = m_currency.parseAccountAddressString(message.address, address);
    if (!extracted) {
      throw std::system_error(make_error_code(cryptonote::error::BAD_ADDRESS));
    }

    context->messages.push_back( { message.message, true, address } );
  }

  if(context->mixIn != 0) {
    return makeGetRandomOutsRequest(std::move(context), false);
  }

  return doSendTransaction(std::move(context), events);
}

std::unique_ptr<WalletRequest> WalletTransactionSender::makeDepositRequest(TransactionId& transactionId, std::deque<std::unique_ptr<WalletEvent>>& events,
    uint64_t term, uint64_t amount, uint64_t fee, uint64_t mixIn) {

  throwIf(term < m_currency.depositMinTerm(), cryptonote::error::DEPOSIT_TERM_TOO_SMALL);
  throwIf(term > m_currency.depositMaxTerm(), cryptonote::error::DEPOSIT_TERM_TOO_BIG);
  throwIf(amount < m_currency.depositMinAmount(), cryptonote::error::DEPOSIT_AMOUNT_TOO_SMALL);

  uint64_t neededMoney = getSumWithOverflowCheck(amount, fee);
  std::shared_ptr<SendTransactionContext> context = std::make_shared<SendTransactionContext>();
  context->dustPolicy.dustThreshold = m_currency.defaultDustThreshold();

  context->foundMoney = selectTransfersToSend(neededMoney, 0 == mixIn, context->dustPolicy.dustThreshold, context->selectedTransfers);

  throwIf(context->foundMoney < neededMoney, cryptonote::error::WRONG_AMOUNT);

  transactionId = m_transactionsCache.addNewTransaction(neededMoney, fee, std::string(), {}, 0, {});
  context->transactionId = transactionId;
  context->mixIn = mixIn;
  context->depositTerm = term;

  if(context->mixIn != 0) {
    return makeGetRandomOutsRequest(std::move(context), true);
  }

  return doSendMultisigTransaction(std::move(context), events);
}

std::unique_ptr<WalletRequest> WalletTransactionSender::makeWithdrawDepositRequest(TransactionId& transactionId, std::deque<std::unique_ptr<WalletEvent>>& events,
    const std::vector<DepositId>& depositIds, uint64_t fee) {

  std::shared_ptr<SendTransactionContext> context = std::make_shared<SendTransactionContext>();
  context->dustPolicy.dustThreshold = m_currency.defaultDustThreshold();

  context->foundMoney = selectDepositTransfers(depositIds, context->selectedTransfers);
  throwIf(context->foundMoney < fee, cryptonote::error::WRONG_AMOUNT);

  transactionId = m_transactionsCache.addNewTransaction(context->foundMoney, fee, std::string(), {}, 0, {});
  context->transactionId = transactionId;
  context->mixIn = 0;

  setSpendingTransactionToDeposits(transactionId, depositIds);

  return doSendDepositWithdrawTransaction(std::move(context), events, depositIds);
}

std::unique_ptr<WalletRequest> WalletTransactionSender::makeGetRandomOutsRequest(std::shared_ptr<SendTransactionContext>&& context, bool isMultisigTransaction) {
  uint64_t outsCount = context->mixIn + 1;// add one to make possible (if need) to skip real output key
  std::vector<uint64_t> amounts;

  for (const auto& td : context->selectedTransfers) {
    amounts.push_back(td.amount);
  }

  return std::unique_ptr<WalletRequest>(new WalletGetRandomOutsByAmountsRequest(amounts, outsCount, context,
    std::bind(&WalletTransactionSender::sendTransactionRandomOutsByAmount, this, isMultisigTransaction, context,
      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
}

void WalletTransactionSender::sendTransactionRandomOutsByAmount(bool isMultisigTransaction, std::shared_ptr<SendTransactionContext> context, std::deque<std::unique_ptr<WalletEvent>>& events,
  std::unique_ptr<WalletRequest>& nextRequest, std::error_code ec) {

  if (m_isStoping) {
    ec = make_error_code(cryptonote::error::TX_CANCELLED);
  }

  if (ec) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, ec));
    return;
  }

  if (!checkIfEnoughMixins(context->outs, context->mixIn)) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(cryptonote::error::MIXIN_COUNT_TOO_BIG)));
    return;
  }

  if (isMultisigTransaction) {
    nextRequest = doSendMultisigTransaction(std::move(context), events);
  } else {
    nextRequest = doSendTransaction(std::move(context), events);
  }
}

bool WalletTransactionSender::checkIfEnoughMixins(const std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs, uint64_t mixIn) {
  auto scanty_it = std::find_if(outs.begin(), outs.end(),
    [&] (const cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& out) {return out.outs.size() < mixIn;});

  if (scanty_it != outs.end()) {
    return false;
  }

  return true;
}

std::unique_ptr<WalletRequest> WalletTransactionSender::doSendTransaction(std::shared_ptr<SendTransactionContext>&& context, std::deque<std::unique_ptr<WalletEvent>>& events) {
  if (m_isStoping) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(cryptonote::error::TX_CANCELLED)));
    return std::unique_ptr<WalletRequest>();
  }

  try
  {
    TransactionInfo& transaction = m_transactionsCache.getTransaction(context->transactionId);

    std::vector<cryptonote::tx_source_entry> sources;
    prepareKeyInputs(context->selectedTransfers, context->outs, sources, context->mixIn);

    cryptonote::tx_destination_entry changeDts = AUTO_VAL_INIT(changeDts);
    uint64_t totalAmount = -transaction.totalAmount;
    createChangeDestinations(m_keys.m_account_address, totalAmount, context->foundMoney, changeDts);

    std::vector<cryptonote::tx_destination_entry> splittedDests;
    splitDestinations(transaction.firstTransferId, transaction.transferCount, changeDts, context->dustPolicy, splittedDests);

    cryptonote::Transaction tx;
    constructTx(m_keys, sources, splittedDests, transaction.extra, transaction.unlockTime, m_upperTransactionSizeLimit, tx, context->messages);

    fillTransactionHash(tx, transaction.hash);

    m_transactionsCache.updateTransaction(context->transactionId, tx, totalAmount, context->selectedTransfers);

    notifyBalanceChanged(events);
   
    return std::unique_ptr<WalletRequest>(new WalletRelayTransactionRequest(tx, std::bind(&WalletTransactionSender::relayTransactionCallback, this, context,
                                                                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
  }
  catch(std::system_error& ec) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, ec.code()));
  }
  catch(std::exception&) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR)));
  }

  return std::unique_ptr<WalletRequest>();
}

std::unique_ptr<WalletRequest> WalletTransactionSender::doSendMultisigTransaction(std::shared_ptr<SendTransactionContext>&& context, std::deque<std::unique_ptr<WalletEvent>>& events) {
  if (m_isStoping) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(cryptonote::error::TX_CANCELLED)));
    return std::unique_ptr<WalletRequest>();
  }

  try {
    //TODO decompose this method
    TransactionInfo& transactionInfo = m_transactionsCache.getTransaction(context->transactionId);

    std::unique_ptr<ITransaction> transaction = createTransaction();

    uint64_t totalAmount = std::abs(transactionInfo.totalAmount);
    std::vector<TransactionTypes::InputKeyInfo> inputs = prepareKeyInputs(context->selectedTransfers, context->outs, context->mixIn);
    std::vector<uint64_t> decomposedChange = splitAmount(context->foundMoney - totalAmount, context->dustPolicy.dustThreshold);

    AccountAddress myAddress = convertAccountAddress(m_keys.m_account_address);
    auto depositIndex = transaction->addOutput(std::abs(transactionInfo.totalAmount) - transactionInfo.fee,
      { myAddress },
      1,
      context->depositTerm);

    for (uint64_t changeOut: decomposedChange) {
      transaction->addOutput(changeOut, myAddress);
    }

    transaction->setUnlockTime(transactionInfo.unlockTime);

    std::vector<KeyPair> ephKeys;
    ephKeys.reserve(inputs.size());

    for (const auto& input: inputs) {
      KeyPair ephKey;
      transaction->addInput(convertAccountKeys(m_keys), input, ephKey);
      ephKeys.push_back(std::move(ephKey));
    }

    for (size_t i = 0; i < inputs.size(); ++i) {
      transaction->signInputKey(i, inputs[i], ephKeys[i]);
    }

    transactionInfo.hash = transaction->getTransactionHash();

    Deposit deposit;
    deposit.amount = std::abs(transactionInfo.totalAmount) - transactionInfo.fee;
    deposit.term = context->depositTerm;
    deposit.creatingTransactionId = context->transactionId;
    deposit.spendingTransactionId = INVALID_TRANSACTION_ID;
    deposit.interest = m_currency.calculateInterest(deposit.amount, deposit.term);
    deposit.locked = true;
    DepositId depositId = m_transactionsCache.insertDeposit(deposit, depositIndex, transaction->getTransactionHash());
    transactionInfo.firstDepositId = depositId;
    transactionInfo.depositCount = 1;

    cryptonote::Transaction lowlevelTransaction = convertTransaction(*transaction);
    m_transactionsCache.updateTransaction(context->transactionId, lowlevelTransaction, totalAmount, context->selectedTransfers);
    m_transactionsCache.addCreatedDeposit(depositId, deposit.amount + deposit.interest);

    notifyBalanceChanged(events);

    std::vector<DepositId> deposits {depositId};

    return std::unique_ptr<WalletRequest>(new WalletRelayDepositTransactionRequest(lowlevelTransaction, std::bind(&WalletTransactionSender::relayDepositTransactionCallback, this, context,
                                                                                    deposits, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));

  } catch(std::system_error& ec) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, ec.code()));
  } catch(std::exception&) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR)));
  }

  return std::unique_ptr<WalletRequest>();
}

std::unique_ptr<WalletRequest> WalletTransactionSender::doSendDepositWithdrawTransaction(std::shared_ptr<SendTransactionContext>&& context,
  std::deque<std::unique_ptr<WalletEvent>>& events, const std::vector<DepositId>& depositIds) {
  if (m_isStoping) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(cryptonote::error::TX_CANCELLED)));
    return std::unique_ptr<WalletRequest>();
  }

  try {
    TransactionInfo& transactionInfo = m_transactionsCache.getTransaction(context->transactionId);

    std::unique_ptr<ITransaction> transaction = createTransaction();
    std::vector<TransactionTypes::InputMultisignature> inputs = prepareMultisignatureInputs(context->selectedTransfers);

    std::vector<uint64_t> outputAmounts = splitAmount(context->foundMoney - transactionInfo.fee, context->dustPolicy.dustThreshold);

    for (const auto& input: inputs) {
      transaction->addInput(input);
    }

    AccountAddress myAddress = convertAccountAddress(m_keys.m_account_address);
    for (auto amount: outputAmounts) {
      transaction->addOutput(amount, myAddress);
    }

    transaction->setUnlockTime(transactionInfo.unlockTime);

    assert(inputs.size() == context->selectedTransfers.size());
    AccountKeys myKeys = convertAccountKeys(m_keys);
    for (size_t i = 0; i < inputs.size(); ++i) {
      transaction->signInputMultisignature(i, context->selectedTransfers[i].transactionPublicKey, context->selectedTransfers[i].outputInTransaction, myKeys);
    }

    transactionInfo.hash = transaction->getTransactionHash();

    cryptonote::Transaction lowlevelTransaction = convertTransaction(*transaction);

    uint64_t interestsSum;
    uint64_t totalSum;
    countDepositsTotalSumAndInterestSum(depositIds, m_transactionsCache, totalSum, interestsSum);

    UnconfirmedSpentDepositDetails unconfirmed;
    unconfirmed.depositsSum  = totalSum;
    unconfirmed.fee = transactionInfo.fee;
    unconfirmed.transactionId = context->transactionId;
    m_transactionsCache.addDepositSpendingTransaction(transaction->getTransactionHash(), unconfirmed);

    return std::unique_ptr<WalletRelayDepositTransactionRequest>(new WalletRelayDepositTransactionRequest(lowlevelTransaction,
      std::bind(&WalletTransactionSender::relayDepositTransactionCallback, this, context, depositIds, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
  } catch(std::system_error& ec) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, ec.code()));
  } catch(std::exception&) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR)));
  }

  return std::unique_ptr<WalletRequest>();
}

void WalletTransactionSender::relayTransactionCallback(std::shared_ptr<SendTransactionContext> context, std::deque<std::unique_ptr<WalletEvent>>& events,
                                                        std::unique_ptr<WalletRequest>& nextRequest, std::error_code ec) {
  if (m_isStoping) {
    return;
  }

  events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, ec));
}

void WalletTransactionSender::relayDepositTransactionCallback(std::shared_ptr<SendTransactionContext> context, std::vector<DepositId> deposits,
  std::deque<std::unique_ptr<WalletEvent>>& events, std::unique_ptr<WalletRequest>& nextRequest, std::error_code ec) {

  if (m_isStoping) {
    return;
  }

  events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, ec));
  events.push_back(std::unique_ptr<WalletDepositsUpdatedEvent>(new WalletDepositsUpdatedEvent(std::move(deposits))));
}

void WalletTransactionSender::splitDestinations(TransferId firstTransferId, size_t transfersCount, const cryptonote::tx_destination_entry& changeDts,
                                                const TxDustPolicy& dustPolicy, std::vector<cryptonote::tx_destination_entry>& splittedDests) {
  uint64_t dust = 0;

  digitSplitStrategy(firstTransferId, transfersCount, changeDts, dustPolicy.dustThreshold, splittedDests, dust);

  throwIf(dustPolicy.dustThreshold < dust, cryptonote::error::INTERNAL_WALLET_ERROR);
  if (0 != dust && !dustPolicy.addToFee) {
    splittedDests.push_back(cryptonote::tx_destination_entry(dust, dustPolicy.addrForDust));
  }
}


void WalletTransactionSender::digitSplitStrategy(TransferId firstTransferId, size_t transfersCount,
  const cryptonote::tx_destination_entry& change_dst, uint64_t dust_threshold,
  std::vector<cryptonote::tx_destination_entry>& splitted_dsts, uint64_t& dust) {
  splitted_dsts.clear();
  dust = 0;

  for (TransferId idx = firstTransferId; idx < firstTransferId + transfersCount; ++idx) {
    Transfer& de = m_transactionsCache.getTransfer(idx);

    cryptonote::AccountPublicAddress addr;
    if (!m_currency.parseAccountAddressString(de.address, addr)) {
      throw std::system_error(make_error_code(cryptonote::error::BAD_ADDRESS));
    }

    cryptonote::decompose_amount_into_digits(de.amount, dust_threshold,
      [&](uint64_t chunk) { splitted_dsts.push_back(cryptonote::tx_destination_entry(chunk, addr)); },
      [&](uint64_t a_dust) { splitted_dsts.push_back(cryptonote::tx_destination_entry(a_dust, addr)); } );
  }

  cryptonote::decompose_amount_into_digits(change_dst.amount, dust_threshold,
    [&](uint64_t chunk) { splitted_dsts.push_back(cryptonote::tx_destination_entry(chunk, change_dst.addr)); },
    [&](uint64_t a_dust) { dust = a_dust; } );
}


void WalletTransactionSender::prepareKeyInputs(
  const std::vector<TransactionOutputInformation>& selectedTransfers,
  std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs,
  std::vector<cryptonote::tx_source_entry>& sources, uint64_t mixIn) {

  size_t i = 0;

  for (const auto& td: selectedTransfers) {
    assert(td.type == TransactionTypes::OutputType::Key);

    sources.resize(sources.size()+1);
    cryptonote::tx_source_entry& src = sources.back();

    src.amount = td.amount;

    //paste mixin transaction
    if(outs.size()) {
      outs[i].outs.sort([](const cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& a, const cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& b){return a.global_amount_index < b.global_amount_index;});
      for (auto& daemon_oe: outs[i].outs) {
        if(td.globalOutputIndex == daemon_oe.global_amount_index)
          continue;
        cryptonote::tx_source_entry::output_entry oe;
        oe.first = daemon_oe.global_amount_index;
        oe.second = daemon_oe.out_key;
        src.outputs.push_back(oe);
        if(src.outputs.size() >= mixIn)
          break;
      }
    }

    //paste real transaction to the random index
    auto it_to_insert = std::find_if(src.outputs.begin(), src.outputs.end(), [&](const cryptonote::tx_source_entry::output_entry& a) { return a.first >= td.globalOutputIndex; });

    cryptonote::tx_source_entry::output_entry real_oe;
    real_oe.first = td.globalOutputIndex;
    real_oe.second = reinterpret_cast<const crypto::public_key&>(td.outputKey);

    auto interted_it = src.outputs.insert(it_to_insert, real_oe);

    src.real_out_tx_key = reinterpret_cast<const crypto::public_key&>(td.transactionPublicKey);
    src.real_output = interted_it - src.outputs.begin();
    src.real_output_in_tx_index = td.outputInTransaction;
    ++i;
  }
}

std::vector<TransactionTypes::InputKeyInfo> WalletTransactionSender::prepareKeyInputs(
  const std::vector<TransactionOutputInformation>& selectedTransfers,
  std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs, uint64_t mixIn) {

  std::vector<cryptonote::tx_source_entry> sources;
  prepareKeyInputs(selectedTransfers, outs, sources, mixIn);

  return convertSources(std::move(sources));
}

std::vector<TransactionTypes::InputMultisignature> WalletTransactionSender::prepareMultisignatureInputs(const std::vector<TransactionOutputInformation>& selectedTransfers) {
  std::vector<TransactionTypes::InputMultisignature> inputs;
  inputs.reserve(selectedTransfers.size());

  for (const auto& output: selectedTransfers) {
    assert(output.type == TransactionTypes::OutputType::Multisignature);
    assert(output.requiredSignatures == 1); //Other types are currently unsupported

    TransactionTypes::InputMultisignature input;
    input.amount = output.amount;
    input.signatures = output.requiredSignatures;
    input.outputIndex = output.globalOutputIndex;
    input.term = output.term;

    inputs.emplace_back(std::move(input));
  }

  return inputs;
}

void WalletTransactionSender::notifyBalanceChanged(std::deque<std::unique_ptr<WalletEvent>>& events) {
  uint64_t unconfirmedOutsAmount = m_transactionsCache.unconfrimedOutsAmount();
  uint64_t change = unconfirmedOutsAmount - m_transactionsCache.unconfirmedTransactionsAmount();

  uint64_t actualBalance = m_transferDetails.balance(ITransfersContainer::IncludeKeyUnlocked) - unconfirmedOutsAmount;
  uint64_t pendingBalance = m_transferDetails.balance(ITransfersContainer::IncludeKeyNotUnlocked) + change;

  events.push_back(std::unique_ptr<WalletActualBalanceUpdatedEvent>(new WalletActualBalanceUpdatedEvent(actualBalance)));
  events.push_back(std::unique_ptr<WalletPendingBalanceUpdatedEvent>(new WalletPendingBalanceUpdatedEvent(pendingBalance)));
}

namespace {

template<typename URNG, typename T>
T popRandomValue(URNG& randomGenerator, std::vector<T>& vec) {
  CHECK_AND_ASSERT_MES(!vec.empty(), T(), "Vector must be non-empty");

  std::uniform_int_distribution<size_t> distribution(0, vec.size() - 1);
  size_t idx = distribution(randomGenerator);

  T res = vec[idx];
  if (idx + 1 != vec.size()) {
    vec[idx] = vec.back();
  }
  vec.resize(vec.size() - 1);

  return res;
}

}

uint64_t WalletTransactionSender::selectTransfersToSend(uint64_t neededMoney, bool addDust, uint64_t dust, std::vector<TransactionOutputInformation>& selectedTransfers) {
  std::vector<size_t> unusedTransfers;
  std::vector<size_t> unusedDust;

  std::vector<TransactionOutputInformation> outputs;
  m_transferDetails.getOutputs(outputs, ITransfersContainer::IncludeKeyUnlocked);

  for (size_t i = 0; i < outputs.size(); ++i) {
    const auto& out = outputs[i];
    if (!m_transactionsCache.isUsed(out)) {
      if (dust < out.amount)
        unusedTransfers.push_back(i);
      else
        unusedDust.push_back(i);
    }
  }

  std::default_random_engine randomGenerator(crypto::rand<std::default_random_engine::result_type>());
  bool selectOneDust = addDust && !unusedDust.empty();
  uint64_t foundMoney = 0;

  while (foundMoney < neededMoney && (!unusedTransfers.empty() || !unusedDust.empty())) {
    size_t idx;
    if (selectOneDust) {
      idx = popRandomValue(randomGenerator, unusedDust);
      selectOneDust = false;
    } else {
      idx = !unusedTransfers.empty() ? popRandomValue(randomGenerator, unusedTransfers) : popRandomValue(randomGenerator, unusedDust);
    }

    selectedTransfers.push_back(outputs[idx]);
    foundMoney += outputs[idx].amount;
  }

  return foundMoney;
}

uint64_t WalletTransactionSender::selectDepositTransfers(const std::vector<DepositId>& depositIds, std::vector<TransactionOutputInformation>& selectedTransfers) {
  uint64_t foundMoney = 0;

  for (auto id: depositIds) {
    Hash transactionHash;
    uint32_t outputInTransaction;
    throwIf(m_transactionsCache.getDepositInTransactionInfo(id, transactionHash, outputInTransaction) == false, cryptonote::error::DEPOSIT_DOESNOT_EXIST);

    {
      TransactionOutputInformation transfer;
      ITransfersContainer::TransferState state;
      throwIf(m_transferDetails.getTransfer(transactionHash, outputInTransaction, transfer, state) == false, cryptonote::error::DEPOSIT_DOESNOT_EXIST);
      throwIf(state != ITransfersContainer::TransferState::TransferAvailable, cryptonote::error::DEPOSIT_LOCKED);

      selectedTransfers.push_back(std::move(transfer));
    }

    Deposit deposit;
    bool r = m_transactionsCache.getDeposit(id, deposit);
    assert(r);

    foundMoney += deposit.amount + deposit.interest;
  }

  return foundMoney;
}

void WalletTransactionSender::setSpendingTransactionToDeposits(TransactionId transactionId, const std::vector<DepositId>& depositIds) {
  for (auto id: depositIds) {
    Deposit& deposit = m_transactionsCache.getDeposit(id);
    deposit.spendingTransactionId = transactionId;
  }
}

} /* namespace CryptoNote */
