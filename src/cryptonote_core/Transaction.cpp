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

#include "ITransaction.h"
#include "TransactionExtra.h"

#include "cryptonote_format_utils.h"
#include "account.h"

#include <boost/optional.hpp>
#include <numeric>
#include <unordered_set>

namespace {

  using namespace cryptonote;
  using namespace CryptoNote;

  void derivePublicKey(const AccountAddress& to, const crypto::secret_key& txKey, size_t outputIndex, crypto::public_key& ephemeralKey) {
    crypto::key_derivation derivation;
    crypto::generate_key_derivation(*reinterpret_cast<const crypto::public_key*>(&to.viewPublicKey), txKey, derivation);
    crypto::derive_public_key(derivation, outputIndex, *reinterpret_cast<const crypto::public_key*>(&to.spendPublicKey), ephemeralKey);
  }

  bool checkInputsKeyimagesDiff(const cryptonote::Transaction& tx) { 
    std::unordered_set<crypto::key_image> ki;
    for (const auto& in : tx.vin) {
      if (in.type() == typeid(TransactionInputToKey)) {
        if (!ki.insert(boost::get<TransactionInputToKey>(in).keyImage).second)
          return false;
      }
    }
    return true;
  }


  // TransactionInput helper functions

  size_t getRequiredSignaturesCount(const TransactionInput& in) {
    if (in.type() == typeid(TransactionInputToKey)) {
      return boost::get<TransactionInputToKey>(in).keyOffsets.size();
    }
    if (in.type() == typeid(TransactionInputMultisignature)) {
      return boost::get<TransactionInputMultisignature>(in).signatures;
    }
    return 0;
  }

  uint64_t getTransactionInputAmount(const TransactionInput& in) {
    if (in.type() == typeid(TransactionInputToKey)) {
      return boost::get<TransactionInputToKey>(in).amount;
    }
    if (in.type() == typeid(TransactionInputMultisignature)) {
      return boost::get<TransactionInputMultisignature>(in).amount;
    }
    return 0;
  }

  TransactionTypes::InputType getTransactionInputType(const TransactionInput& in) {
    if (in.type() == typeid(TransactionInputToKey)) {
      return TransactionTypes::InputType::Key;
    }
    if (in.type() == typeid(TransactionInputMultisignature)) {
      return TransactionTypes::InputType::Multisignature;
    }
    if (in.type() == typeid(TransactionInputGenerate)) {
      return TransactionTypes::InputType::Generating;
    }
    return TransactionTypes::InputType::Invalid;
  }

  const TransactionInput& getInputChecked(const cryptonote::Transaction& transaction, size_t index) {
    if (transaction.vin.size() <= index) {
      throw std::runtime_error("Transaction input index out of range");
    }
    return transaction.vin[index];
  }

  const TransactionInput& getInputChecked(const cryptonote::Transaction& transaction, size_t index, TransactionTypes::InputType type) {
    const auto& input = getInputChecked(transaction, index);
    if (getTransactionInputType(input) != type) {
      throw std::runtime_error("Unexpected transaction input type");
    }
    return input;
  }

  // TransactionOutput helper functions

  TransactionTypes::OutputType getTransactionOutputType(const TransactionOutputTarget& out) {
    if (out.type() == typeid(TransactionOutputToKey)) {
      return TransactionTypes::OutputType::Key;
    }
    if (out.type() == typeid(TransactionOutputMultisignature)) {
      return TransactionTypes::OutputType::Multisignature;
    }
    return TransactionTypes::OutputType::Invalid;
  }

  const TransactionOutput& getOutputChecked(const cryptonote::Transaction& transaction, size_t index) {
    if (transaction.vout.size() <= index) {
      throw std::runtime_error("Transaction output index out of range");
    }
    return transaction.vout[index];
  }

  const TransactionOutput& getOutputChecked(const cryptonote::Transaction& transaction, size_t index, TransactionTypes::OutputType type) {
    const auto& output = getOutputChecked(transaction, index);
    if (getTransactionOutputType(output.target) != type) {
      throw std::runtime_error("Unexpected transaction output target type");
    }
    return output;
  }
}


namespace CryptoNote {

  using namespace TransactionTypes;

  ////////////////////////////////////////////////////////////////////////
  // class Transaction declaration
  ////////////////////////////////////////////////////////////////////////

  class Transaction : public ITransaction {
  public:
    Transaction();
    Transaction(const Blob& txblob);
    Transaction(const cryptonote::Transaction& tx);
  
    // ITransactionReader
    virtual Hash getTransactionHash() const override;
    virtual Hash getTransactionPrefixHash() const override;
    virtual PublicKey getTransactionPublicKey() const override;
    virtual uint64_t getUnlockTime() const override;
    virtual bool getPaymentId(Hash& hash) const override;
    virtual bool getExtraNonce(std::string& nonce) const override;

    // inputs
    virtual size_t getInputCount() const override;
    virtual uint64_t getInputTotalAmount() const override;
    virtual TransactionTypes::InputType getInputType(size_t index) const override;
    virtual void getInput(size_t index, TransactionTypes::InputKey& input) const override;
    virtual void getInput(size_t index, TransactionTypes::InputMultisignature& input) const override;

    // outputs
    virtual size_t getOutputCount() const override;
    virtual uint64_t getOutputTotalAmount() const override;
    virtual TransactionTypes::OutputType getOutputType(size_t index) const override;
    virtual void getOutput(size_t index, TransactionTypes::OutputKey& output) const override;
    virtual void getOutput(size_t index, TransactionTypes::OutputMultisignature& output) const override;

    virtual size_t getRequiredSignaturesCount(size_t index) const override;
    virtual bool findOutputsToAccount(const AccountAddress& addr, const SecretKey& viewSecretKey, std::vector<uint32_t>& outs, uint64_t& outputAmount) const override;

    // various checks
    virtual bool validateInputs() const override;
    virtual bool validateOutputs() const override;
    virtual bool validateSignatures() const override;

    // get serialized transaction
    virtual Blob getTransactionData() const override;

    // ITransactionWriter

    virtual void setUnlockTime(uint64_t unlockTime) override;
    virtual void setPaymentId(const Hash& hash) override;
    virtual void setExtraNonce(const std::string& nonce) override;

    // Inputs/Outputs 
    virtual size_t addInput(const TransactionTypes::InputKey& input) override;
    virtual size_t addInput(const AccountKeys& senderKeys, const TransactionTypes::InputKeyInfo& info, KeyPair& ephKeys) override;
    virtual size_t addInput(const TransactionTypes::InputMultisignature& input) override;

    virtual size_t addOutput(uint64_t amount, const AccountAddress& to) override;
    virtual size_t addOutput(uint64_t amount, const std::vector<AccountAddress>& to, uint32_t requiredSignatures) override;

    virtual void signInputKey(size_t input, const TransactionTypes::InputKeyInfo& info, const KeyPair& ephKeys) override;
    virtual void signInputMultisignature(size_t input, const PublicKey& sourceTransactionKey, size_t outputIndex, const AccountKeys& accountKeys) override;

    // secret key
    virtual bool getTransactionSecretKey(SecretKey& key) const override;
    virtual void setTransactionSecretKey(const SecretKey& key) override;

  private:

    std::vector<crypto::signature>& getSignatures(size_t input);

    const crypto::secret_key& txSecretKey() const {
      if (!secretKey) {
        throw std::runtime_error("Operation requires transaction secret key");
      }
      return *secretKey;
    }

    cryptonote::Transaction constructFinalTransaction() const {
      cryptonote::Transaction tx(transaction);
      tx.extra = extra.serialize();
      return tx;
    }

    void checkIfSigning() const {
      if (!transaction.signatures.empty()) {
        throw std::runtime_error("Cannot perform requested operation, since it will invalidate transaction signatures");
      }
    }

    cryptonote::Transaction transaction;
    boost::optional<crypto::secret_key> secretKey;
    TransactionExtra extra;
  };


  ////////////////////////////////////////////////////////////////////////
  // class Transaction implementation
  ////////////////////////////////////////////////////////////////////////

  std::unique_ptr<ITransaction> createTransaction() {
    return std::unique_ptr<ITransaction>(new Transaction());
  }

  std::unique_ptr<ITransaction> createTransaction(const Blob& transactionBlob) {
    return std::unique_ptr<ITransaction>(new Transaction(transactionBlob));
  }

  std::unique_ptr<ITransaction> createTransaction(const cryptonote::Transaction& tx) {
    return std::unique_ptr<ITransaction>(new Transaction(tx));
  }

  Transaction::Transaction() {   
    cryptonote::KeyPair txKeys(cryptonote::KeyPair::generate());

    transaction.version = CURRENT_TRANSACTION_VERSION;
    transaction.unlockTime = 0;

    tx_extra_pub_key pk = { txKeys.pub };
    extra.set(pk);

    secretKey = txKeys.sec;
  }

  Transaction::Transaction(const Blob& data) {
    cryptonote::blobdata blob(reinterpret_cast<const char*>(data.data()), data.size());
    if (!cryptonote::parse_and_validate_tx_from_blob(blob, transaction)) {
      throw std::runtime_error("Invalid transaction data");
    }

    extra.parse(transaction.extra);
  }

  Transaction::Transaction(const cryptonote::Transaction& tx) : transaction(tx) {
    extra.parse(transaction.extra);
  }

  Hash Transaction::getTransactionHash() const {
    auto hash = get_transaction_hash(constructFinalTransaction());
    return reinterpret_cast<const Hash&>(hash);
  }

  Hash Transaction::getTransactionPrefixHash() const {
    auto hash = get_transaction_prefix_hash(constructFinalTransaction());
    return reinterpret_cast<const Hash&>(hash);
  }

  PublicKey Transaction::getTransactionPublicKey() const {
    crypto::public_key pk(null_pkey);
    extra.getPublicKey(pk);
    return reinterpret_cast<const PublicKey&>(pk);
  }

  uint64_t Transaction::getUnlockTime() const {
    return transaction.unlockTime;
  }

  void Transaction::setUnlockTime(uint64_t unlockTime) {
    checkIfSigning();
    transaction.unlockTime = unlockTime;
  }

  bool Transaction::getTransactionSecretKey(SecretKey& key) const {
    if (!secretKey) {
      return false;
    }
    key = reinterpret_cast<const SecretKey&>(secretKey.get());
    return true;
  }

  void Transaction::setTransactionSecretKey(const SecretKey& key) {
    const auto& sk = reinterpret_cast<const crypto::secret_key&>(key);
    crypto::public_key pk;
    crypto::public_key txPubKey;

    crypto::secret_key_to_public_key(sk, pk);
    extra.getPublicKey(txPubKey);

    if (txPubKey != pk) {
      throw std::runtime_error("Secret transaction key does not match public key");
    }

    secretKey = reinterpret_cast<const crypto::secret_key&>(key);
  }

  size_t Transaction::addInput(const InputKey& input) {
    checkIfSigning();
    TransactionInputToKey inKey = { input.amount, input.keyOffsets, *reinterpret_cast<const crypto::key_image*>(&input.keyImage) };
    transaction.vin.emplace_back(inKey);
    return transaction.vin.size() - 1;
  }

  size_t Transaction::addInput(const AccountKeys& senderKeys, const TransactionTypes::InputKeyInfo& info, KeyPair& ephKeys) {
    checkIfSigning();
    InputKey input;
    input.amount = info.amount;

    generate_key_image_helper(
      reinterpret_cast<const account_keys&>(senderKeys),
      reinterpret_cast<const crypto::public_key&>(info.realOutput.transactionPublicKey),
      info.realOutput.outputInTransaction,
      reinterpret_cast<cryptonote::KeyPair&>(ephKeys),
      reinterpret_cast<crypto::key_image&>(input.keyImage));

    // fill outputs array and use relative offsets
    for (const auto& out : info.outputs) {
      input.keyOffsets.push_back(out.outputIndex);
    }
    input.keyOffsets = absolute_output_offsets_to_relative(input.keyOffsets);

    return addInput(input);
  }

  size_t Transaction::addInput(const InputMultisignature& input) {
    checkIfSigning();
    TransactionInputMultisignature inMsig;
    inMsig.amount = input.amount;
    inMsig.outputIndex = input.outputIndex;
    inMsig.signatures = input.signatures;
    transaction.vin.push_back(inMsig);
    return transaction.vin.size() - 1;
  }

  size_t Transaction::addOutput(uint64_t amount, const AccountAddress& to) {
    checkIfSigning();
    TransactionOutputToKey outKey;
    derivePublicKey(to, txSecretKey(), transaction.vout.size(), outKey.key);
    TransactionOutput out = { amount, outKey };
    transaction.vout.emplace_back(out);
    return transaction.vout.size() - 1;
  }

  size_t Transaction::addOutput(uint64_t amount, const std::vector<AccountAddress>& to, uint32_t requiredSignatures) {   
    checkIfSigning();
    const auto& txKey = txSecretKey();
    size_t outputIndex = transaction.vout.size();
    TransactionOutputMultisignature outMsig;
    outMsig.requiredSignatures = requiredSignatures;
    outMsig.keys.resize(to.size());
    for (int i = 0; i < to.size(); ++i) {
      derivePublicKey(to[i], txKey, outputIndex, outMsig.keys[i]);
    }
    TransactionOutput out = { amount, outMsig };
    transaction.vout.emplace_back(out);
    return outputIndex;
  }

  void Transaction::signInputKey(size_t index, const TransactionTypes::InputKeyInfo& info, const KeyPair& ephKeys) {
    const auto& input = boost::get<TransactionInputToKey>(getInputChecked(transaction, index, InputType::Key));
    Hash prefixHash = getTransactionPrefixHash();

    std::vector<crypto::signature> signatures;
    std::vector<const crypto::public_key*> keysPtrs;

    for (const auto& o : info.outputs) {
      keysPtrs.push_back(reinterpret_cast<const crypto::public_key*>(&o.targetKey));
    }

    signatures.resize(keysPtrs.size());

    generate_ring_signature(
      reinterpret_cast<const crypto::hash&>(prefixHash),
      reinterpret_cast<const crypto::key_image&>(input.keyImage),
      keysPtrs,
      reinterpret_cast<const crypto::secret_key&>(ephKeys.secretKey),
      info.realOutput.transactionIndex,
      signatures.data());

    getSignatures(index) = signatures;
  }

  void Transaction::signInputMultisignature(size_t index, const PublicKey& sourceTransactionKey, size_t outputIndex, const AccountKeys& accountKeys) {
    crypto::key_derivation derivation;
    crypto::public_key ephemeralPublicKey;
    crypto::secret_key ephemeralSecretKey;

    crypto::generate_key_derivation(
      reinterpret_cast<const crypto::public_key&>(sourceTransactionKey),
      reinterpret_cast<const crypto::secret_key&>(accountKeys.viewSecretKey),
      derivation);

    crypto::derive_public_key(derivation, outputIndex,
      reinterpret_cast<const crypto::public_key&>(accountKeys.address.spendPublicKey), ephemeralPublicKey);
    crypto::derive_secret_key(derivation, outputIndex,
      reinterpret_cast<const crypto::secret_key&>(accountKeys.spendSecretKey), ephemeralSecretKey);

    crypto::signature signature;
    auto txPrefixHash = getTransactionPrefixHash();

    crypto::generate_signature(reinterpret_cast<const crypto::hash&>(txPrefixHash),
      ephemeralPublicKey, ephemeralSecretKey, signature);

    getSignatures(index).push_back(signature);
  }

  std::vector<crypto::signature>& Transaction::getSignatures(size_t input) {
    // update signatures container size if needed
    if (transaction.signatures.size() < transaction.vin.size()) {
      transaction.signatures.resize(transaction.vin.size());
    }
    // check range
    if (input >= transaction.signatures.size()) {
      throw std::runtime_error("Invalid input index");
    }

    return transaction.signatures[input];
  }

  std::vector<uint8_t> Transaction::getTransactionData() const {
    return stringToVector(t_serializable_object_to_blob(constructFinalTransaction()));
  }

  void Transaction::setPaymentId(const Hash& hash) {
    checkIfSigning();
    blobdata paymentIdBlob;
    set_payment_id_to_tx_extra_nonce(paymentIdBlob, reinterpret_cast<const crypto::hash&>(hash));
    setExtraNonce(paymentIdBlob);
  }

  bool Transaction::getPaymentId(Hash& hash) const {
    blobdata nonce;
    if (getExtraNonce(nonce)) {
      crypto::hash paymentId;
      if (get_payment_id_from_tx_extra_nonce(nonce, paymentId)) {
        hash = reinterpret_cast<const Hash&>(paymentId);
        return true;
      }
    }
    return false;
  }

  void Transaction::setExtraNonce(const std::string& nonce) {
    checkIfSigning();
    tx_extra_nonce extraNonce = { nonce };
    extra.set(extraNonce);
  }

  bool Transaction::getExtraNonce(std::string& nonce) const {
    tx_extra_nonce extraNonce;
    if (extra.get(extraNonce)) {
      nonce = extraNonce.nonce;
      return true;
    }
    return false;
  }

  size_t Transaction::getInputCount() const {
    return transaction.vin.size();
  }

  uint64_t Transaction::getInputTotalAmount() const {
    return std::accumulate(transaction.vin.begin(), transaction.vin.end(), 0ULL, [](uint64_t val, const TransactionInput& in) {
      return val + getTransactionInputAmount(in); });
  }

  TransactionTypes::InputType Transaction::getInputType(size_t index) const {
    return getTransactionInputType(getInputChecked(transaction, index));
  }

  void Transaction::getInput(size_t index, InputKey& input) const {
    const auto& k = boost::get<TransactionInputToKey>(getInputChecked(transaction, index, InputType::Key));
    input.amount = k.amount;
    input.keyImage = reinterpret_cast<const KeyImage&>(k.keyImage);
    input.keyOffsets = k.keyOffsets;
  }

  void Transaction::getInput(size_t index, InputMultisignature& input) const {
    const auto& m = boost::get<TransactionInputMultisignature>(getInputChecked(transaction, index, InputType::Multisignature));
    input.amount = m.amount;
    input.outputIndex = m.outputIndex;
    input.signatures = m.signatures;
  }

  size_t Transaction::getOutputCount() const {
    return transaction.vout.size();
  }

  uint64_t Transaction::getOutputTotalAmount() const {
    return std::accumulate(transaction.vout.begin(), transaction.vout.end(), 0ULL, [](uint64_t val, const TransactionOutput& out) {
      return val + out.amount; });
  }

  TransactionTypes::OutputType Transaction::getOutputType(size_t index) const {
    return getTransactionOutputType(getOutputChecked(transaction, index).target);
  }

  void Transaction::getOutput(size_t index, OutputKey& output) const {
    const auto& out = getOutputChecked(transaction, index, OutputType::Key);
    const auto& k = boost::get<TransactionOutputToKey>(out.target);
    output.amount = out.amount;
    output.key = reinterpret_cast<const PublicKey&>(k.key);
  }

  void Transaction::getOutput(size_t index, OutputMultisignature& output) const {
    const auto& out = getOutputChecked(transaction, index, OutputType::Multisignature);
    const auto& m = boost::get<TransactionOutputMultisignature>(out.target);
    output.amount = out.amount;
    output.keys = reinterpret_cast<const std::vector<PublicKey>&>(m.keys);
    output.requiredSignatures = m.requiredSignatures;
  }

  bool isOutToKey(const crypto::public_key& spendPublicKey, const crypto::public_key& outKey, const crypto::key_derivation& derivation, size_t keyIndex) {
    crypto::public_key pk;
    derive_public_key(derivation, keyIndex, spendPublicKey, pk);
    return pk == outKey;
  }

  bool Transaction::findOutputsToAccount(const AccountAddress& addr, const SecretKey& viewSecretKey, std::vector<uint32_t>& out, uint64_t& amount) const {
    account_keys keys;
    keys.m_account_address = reinterpret_cast<const AccountPublicAddress&>(addr);
    // only view secret key is used, spend key is not needed
    keys.m_view_secret_key = reinterpret_cast<const crypto::secret_key&>(viewSecretKey);

    auto pk = getTransactionPublicKey();
    crypto::public_key txPubKey = reinterpret_cast<const crypto::public_key&>(pk);

    amount = 0;
    size_t keyIndex = 0;
    uint32_t outputIndex = 0;

    crypto::key_derivation derivation;
    generate_key_derivation(txPubKey, keys.m_view_secret_key, derivation);

    for (const TransactionOutput& o : transaction.vout) {
      assert(o.target.type() == typeid(TransactionOutputToKey) || o.target.type() == typeid(TransactionOutputMultisignature));
      if (o.target.type() == typeid(TransactionOutputToKey)) {
        if (is_out_to_acc(keys, boost::get<TransactionOutputToKey>(o.target), derivation, keyIndex)) {
          out.push_back(outputIndex);
          amount += o.amount;
        }
        ++keyIndex;
      } else if (o.target.type() == typeid(TransactionOutputMultisignature)) {
        const auto& target = boost::get<TransactionOutputMultisignature>(o.target);
        for (const auto& key : target.keys) {
          if (isOutToKey(keys.m_account_address.m_spendPublicKey, key, derivation, static_cast<size_t>(outputIndex))) {
            out.push_back(outputIndex);
          }
          ++keyIndex;
        }
      }
      ++outputIndex;
    }

    return true;
  }

  size_t Transaction::getRequiredSignaturesCount(size_t index) const {
    return ::getRequiredSignaturesCount(getInputChecked(transaction, index));
  }

  bool Transaction::validateInputs() const {
    return
      check_inputs_types_supported(transaction) &&
      check_inputs_overflow(transaction) &&
      checkInputsKeyimagesDiff(transaction) &&
      checkMultisignatureInputsDiff(transaction);
  }

  bool Transaction::validateOutputs() const {
    return
      check_outs_valid(transaction) &&
      check_outs_overflow(transaction);
  }

  bool Transaction::validateSignatures() const {
    if (transaction.signatures.size() < transaction.vin.size()) {
      return false;
    }

    for (size_t i = 0; i < transaction.vin.size(); ++i) {
      if (getRequiredSignaturesCount(i) > transaction.signatures[i].size()) {
        return false;
      }
    }

    return true;
  }
}
