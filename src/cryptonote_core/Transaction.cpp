// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ITransaction.h"
#include "TransactionExtra.h"

#include "cryptonote_format_utils.h"
#include "cryptonote_config.h"
#include "account.h"

#include <boost/optional.hpp>
#include <numeric>
#include <unordered_set>

namespace {

  using namespace CryptoNote;

  void derivePublicKey(const AccountAddress& to, const crypto::secret_key& txKey, size_t outputIndex, crypto::public_key& ephemeralKey) {
    crypto::key_derivation derivation;
    crypto::generate_key_derivation(*reinterpret_cast<const crypto::public_key*>(&to.viewPublicKey), txKey, derivation);
    crypto::derive_public_key(derivation, outputIndex, *reinterpret_cast<const crypto::public_key*>(&to.spendPublicKey), ephemeralKey);
  }

  bool checkInputsKeyimagesDiff(const cryptonote::Transaction& tx) {
    std::unordered_set<crypto::key_image> ki;
    for (const auto& in : tx.vin) {
      if (in.type() == typeid(cryptonote::TransactionInputToKey)) {
        if (!ki.insert(boost::get<cryptonote::TransactionInputToKey>(in).keyImage).second)
          return false;
      }
    }
    return true;
  }


  // TransactionInput helper functions

  size_t getRequiredSignaturesCount(const cryptonote::TransactionInput& in) {
    if (in.type() == typeid(cryptonote::TransactionInputToKey)) {
      return boost::get<cryptonote::TransactionInputToKey>(in).keyOffsets.size();
    }
    if (in.type() == typeid(cryptonote::TransactionInputMultisignature)) {
      return boost::get<cryptonote::TransactionInputMultisignature>(in).signatures;
    }
    return 0;
  }

  uint64_t getTransactionInputAmount(const cryptonote::TransactionInput& in) {
    if (in.type() == typeid(cryptonote::TransactionInputToKey)) {
      return boost::get<cryptonote::TransactionInputToKey>(in).amount;
    }
    if (in.type() == typeid(cryptonote::TransactionInputMultisignature)) {
      // TODO calculate interest
      return boost::get<cryptonote::TransactionInputMultisignature>(in).amount;
    }
    return 0;
  }

  CryptoNote::TransactionTypes::InputType getTransactionInputType(const cryptonote::TransactionInput& in) {
    if (in.type() == typeid(cryptonote::TransactionInputToKey)) {
      return TransactionTypes::InputType::Key;
    }
    if (in.type() == typeid(cryptonote::TransactionInputMultisignature)) {
      return TransactionTypes::InputType::Multisignature;
    }
    if (in.type() == typeid(cryptonote::TransactionInputGenerate)) {
      return TransactionTypes::InputType::Generating;
    }
    return TransactionTypes::InputType::Invalid;
  }

  const cryptonote::TransactionInput& getInputChecked(const cryptonote::Transaction& transaction, size_t index) {
    if (transaction.vin.size() <= index) {
      throw std::runtime_error("Transaction input index out of range");
    }
    return transaction.vin[index];
  }

  const cryptonote::TransactionInput& getInputChecked(const cryptonote::Transaction& transaction, size_t index, TransactionTypes::InputType type) {
    const auto& input = getInputChecked(transaction, index);
    if (getTransactionInputType(input) != type) {
      throw std::runtime_error("Unexpected transaction input type");
    }
    return input;
  }

  // TransactionOutput helper functions

  TransactionTypes::OutputType getTransactionOutputType(const cryptonote::TransactionOutputTarget& out) {
    if (out.type() == typeid(cryptonote::TransactionOutputToKey)) {
      return TransactionTypes::OutputType::Key;
    }
    if (out.type() == typeid(cryptonote::TransactionOutputMultisignature)) {
      return TransactionTypes::OutputType::Multisignature;
    }
    return TransactionTypes::OutputType::Invalid;
  }

  const cryptonote::TransactionOutput& getOutputChecked(const cryptonote::Transaction& transaction, size_t index) {
    if (transaction.vout.size() <= index) {
      throw std::runtime_error("Transaction output index out of range");
    }
    return transaction.vout[index];
  }

  const cryptonote::TransactionOutput& getOutputChecked(const cryptonote::Transaction& transaction, size_t index, TransactionTypes::OutputType type) {
    const auto& output = getOutputChecked(transaction, index);
    if (getTransactionOutputType(output.target) != type) {
      throw std::runtime_error("Unexpected transaction output target type");
    }
    return output;
  }
}


namespace CryptoNote {

  using namespace cryptonote;
  using namespace TransactionTypes;

  ////////////////////////////////////////////////////////////////////////
  // class Transaction declaration
  ////////////////////////////////////////////////////////////////////////

  class TransactionImpl : public ITransaction {
  public:
    TransactionImpl();
    TransactionImpl(const Blob& txblob);
    TransactionImpl(const cryptonote::Transaction& tx);
  
    // ITransactionReader
    virtual Hash getTransactionHash() const override;
    virtual Hash getTransactionPrefixHash() const override;
    virtual PublicKey getTransactionPublicKey() const override;
    virtual uint64_t getUnlockTime() const override;
    virtual std::vector<uint8_t> getExtra() const override;
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
    virtual size_t addOutput(uint64_t amount, const std::vector<AccountAddress>& to, uint32_t requiredSignatures, uint32_t term = 0) override;

    virtual void signInputKey(size_t input, const TransactionTypes::InputKeyInfo& info, const KeyPair& ephKeys) override;
    virtual void signInputMultisignature(size_t input, const PublicKey& sourceTransactionKey, size_t outputIndex, const AccountKeys& accountKeys) override;

    // secret key
    virtual bool getTransactionSecretKey(SecretKey& key) const override;
    virtual void setTransactionSecretKey(const SecretKey& key) override;

  private:

    void invalidateHash();

    std::vector<crypto::signature>& getSignatures(size_t input);

    const crypto::secret_key& txSecretKey() const {
      if (!secretKey) {
        throw std::runtime_error("Operation requires transaction secret key");
      }
      return *secretKey;
    }

    void checkIfSigning() const {
      if (!transaction.signatures.empty()) {
        throw std::runtime_error("Cannot perform requested operation, since it will invalidate transaction signatures");
      }
    }

    cryptonote::Transaction transaction;
    boost::optional<crypto::secret_key> secretKey;
    mutable boost::optional<crypto::hash> transactionHash;
    TransactionExtra extra;
  };


  ////////////////////////////////////////////////////////////////////////
  // class Transaction implementation
  ////////////////////////////////////////////////////////////////////////

  std::unique_ptr<ITransaction> createTransaction() {
    return std::unique_ptr<ITransaction>(new TransactionImpl());
  }

  std::unique_ptr<ITransaction> createTransaction(const Blob& transactionBlob) {
    return std::unique_ptr<ITransaction>(new TransactionImpl(transactionBlob));
  }

  std::unique_ptr<ITransaction> createTransaction(const cryptonote::Transaction& tx) {
    return std::unique_ptr<ITransaction>(new TransactionImpl(tx));
  }

  TransactionImpl::TransactionImpl() {   
    cryptonote::KeyPair txKeys(cryptonote::KeyPair::generate());

    cryptonote::tx_extra_pub_key pk = { txKeys.pub };
    extra.set(pk);

    transaction.version = TRANSACTION_VERSION_1;
    transaction.unlockTime = 0;
    transaction.extra = extra.serialize();

    secretKey = txKeys.sec;
  }

  TransactionImpl::TransactionImpl(const Blob& data) {
    cryptonote::blobdata blob(reinterpret_cast<const char*>(data.data()), data.size());
    if (!parse_and_validate_tx_from_blob(blob, transaction)) {
      throw std::runtime_error("Invalid transaction data");
    }
    
    extra.parse(transaction.extra);
    transactionHash = get_blob_hash(blob); // avoid serialization if we already have blob
  }

  TransactionImpl::TransactionImpl(const cryptonote::Transaction& tx) : transaction(tx) {
    extra.parse(transaction.extra);
  }

  void TransactionImpl::invalidateHash() {
    if (transactionHash.is_initialized()) {
      transactionHash = decltype(transactionHash)();
    }
  }

  Hash TransactionImpl::getTransactionHash() const {
    if (!transactionHash.is_initialized()) {
      transactionHash = get_transaction_hash(transaction);
    }

    return reinterpret_cast<const Hash&>(transactionHash.get());   
  }

  Hash TransactionImpl::getTransactionPrefixHash() const {
    auto hash = get_transaction_prefix_hash(transaction);
    return reinterpret_cast<const Hash&>(hash);
  }

  PublicKey TransactionImpl::getTransactionPublicKey() const {
    crypto::public_key pk(null_pkey);
    extra.getPublicKey(pk);
    return reinterpret_cast<const PublicKey&>(pk);
  }

  uint64_t TransactionImpl::getUnlockTime() const {
    return transaction.unlockTime;
  }

  void TransactionImpl::setUnlockTime(uint64_t unlockTime) {
    checkIfSigning();
    transaction.unlockTime = unlockTime;
    invalidateHash();
  }

  bool TransactionImpl::getTransactionSecretKey(SecretKey& key) const {
    if (!secretKey) {
      return false;
    }
    key = reinterpret_cast<const SecretKey&>(secretKey.get());
    return true;
  }

  void TransactionImpl::setTransactionSecretKey(const SecretKey& key) {
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

  size_t TransactionImpl::addInput(const InputKey& input) {
    checkIfSigning();
    TransactionInputToKey inKey = { input.amount, input.keyOffsets, *reinterpret_cast<const crypto::key_image*>(&input.keyImage) };
    transaction.vin.emplace_back(inKey);
    invalidateHash();
    return transaction.vin.size() - 1;
  }

  size_t TransactionImpl::addInput(const AccountKeys& senderKeys, const TransactionTypes::InputKeyInfo& info, KeyPair& ephKeys) {
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

  size_t TransactionImpl::addInput(const InputMultisignature& input) {
    checkIfSigning();
    
    TransactionInputMultisignature inMsig;
    inMsig.amount = input.amount;
    inMsig.outputIndex = input.outputIndex;
    inMsig.signatures = input.signatures;
    inMsig.term = input.term;
    transaction.vin.push_back(inMsig);
    transaction.version = TRANSACTION_VERSION_2;
    invalidateHash();

    return transaction.vin.size() - 1;
  }

  size_t TransactionImpl::addOutput(uint64_t amount, const AccountAddress& to) {
    checkIfSigning();

    TransactionOutputToKey outKey;
    derivePublicKey(to, txSecretKey(), transaction.vout.size(), outKey.key);
    TransactionOutput out = { amount, outKey };
    transaction.vout.emplace_back(out);
    invalidateHash();

    return transaction.vout.size() - 1;
  }

  size_t TransactionImpl::addOutput(uint64_t amount, const std::vector<AccountAddress>& to, uint32_t requiredSignatures, uint32_t term) {   
    checkIfSigning();

    const auto& txKey = txSecretKey();
    size_t outputIndex = transaction.vout.size();
    TransactionOutputMultisignature outMsig;
    outMsig.requiredSignatures = requiredSignatures;
    outMsig.keys.resize(to.size());
    outMsig.term = term;
    
    for (size_t i = 0; i < to.size(); ++i) {
      derivePublicKey(to[i], txKey, outputIndex, outMsig.keys[i]);
    }

    TransactionOutput out = { amount, outMsig };
    transaction.vout.emplace_back(out);
    transaction.version = TRANSACTION_VERSION_2;
    invalidateHash();

    return outputIndex;
  }

  void TransactionImpl::signInputKey(size_t index, const TransactionTypes::InputKeyInfo& info, const KeyPair& ephKeys) {
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
    invalidateHash();
  }

  void TransactionImpl::signInputMultisignature(size_t index, const PublicKey& sourceTransactionKey, size_t outputIndex, const AccountKeys& accountKeys) {
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
    invalidateHash();
  }

  std::vector<crypto::signature>& TransactionImpl::getSignatures(size_t input) {
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

  std::vector<uint8_t> TransactionImpl::getTransactionData() const {
    return stringToVector(t_serializable_object_to_blob(transaction));
  }

  void TransactionImpl::setPaymentId(const Hash& hash) {
    checkIfSigning();
    blobdata paymentIdBlob;
    set_payment_id_to_tx_extra_nonce(paymentIdBlob, reinterpret_cast<const crypto::hash&>(hash));
    setExtraNonce(paymentIdBlob);
  }

  std::vector<uint8_t> TransactionImpl::getExtra() const {
    return transaction.extra;
  }

  bool TransactionImpl::getPaymentId(Hash& hash) const {
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

  void TransactionImpl::setExtraNonce(const std::string& nonce) {
    checkIfSigning();
    tx_extra_nonce extraNonce = { nonce };
    extra.set(extraNonce);
    transaction.extra = extra.serialize();
    invalidateHash();
  }

  bool TransactionImpl::getExtraNonce(std::string& nonce) const {
    tx_extra_nonce extraNonce;
    if (extra.get(extraNonce)) {
      nonce = extraNonce.nonce;
      return true;
    }
    return false;
  }

  size_t TransactionImpl::getInputCount() const {
    return transaction.vin.size();
  }

  uint64_t TransactionImpl::getInputTotalAmount() const {
    return std::accumulate(transaction.vin.begin(), transaction.vin.end(), 0ULL, [](uint64_t val, const TransactionInput& in) {
      return val + getTransactionInputAmount(in); });
  }

  TransactionTypes::InputType TransactionImpl::getInputType(size_t index) const {
    return getTransactionInputType(getInputChecked(transaction, index));
  }

  void TransactionImpl::getInput(size_t index, InputKey& input) const {
    const auto& k = boost::get<TransactionInputToKey>(getInputChecked(transaction, index, InputType::Key));
    input.amount = k.amount;
    input.keyImage = reinterpret_cast<const KeyImage&>(k.keyImage);
    input.keyOffsets = k.keyOffsets;
  }

  void TransactionImpl::getInput(size_t index, InputMultisignature& input) const {
    const auto& m = boost::get<TransactionInputMultisignature>(getInputChecked(transaction, index, InputType::Multisignature));
    input.amount = m.amount;
    input.outputIndex = m.outputIndex;
    input.signatures = m.signatures;
    input.term = m.term;
  }

  size_t TransactionImpl::getOutputCount() const {
    return transaction.vout.size();
  }

  uint64_t TransactionImpl::getOutputTotalAmount() const {
    return std::accumulate(transaction.vout.begin(), transaction.vout.end(), 0ULL, [](uint64_t val, const TransactionOutput& out) {
      return val + out.amount; });
  }

  TransactionTypes::OutputType TransactionImpl::getOutputType(size_t index) const {
    return getTransactionOutputType(getOutputChecked(transaction, index).target);
  }

  void TransactionImpl::getOutput(size_t index, OutputKey& output) const {
    const auto& out = getOutputChecked(transaction, index, OutputType::Key);
    const auto& k = boost::get<TransactionOutputToKey>(out.target);
    output.amount = out.amount;
    output.key = reinterpret_cast<const PublicKey&>(k.key);
  }

  void TransactionImpl::getOutput(size_t index, OutputMultisignature& output) const {
    const auto& out = getOutputChecked(transaction, index, OutputType::Multisignature);
    const auto& m = boost::get<TransactionOutputMultisignature>(out.target);
    output.amount = out.amount;
    output.keys = reinterpret_cast<const std::vector<PublicKey>&>(m.keys);
    output.requiredSignatures = m.requiredSignatures;
    output.term = m.term;
  }

  bool isOutToKey(const crypto::public_key& spendPublicKey, const crypto::public_key& outKey, const crypto::key_derivation& derivation, size_t keyIndex) {
    crypto::public_key pk;
    derive_public_key(derivation, keyIndex, spendPublicKey, pk);
    return pk == outKey;
  }

  bool TransactionImpl::findOutputsToAccount(const AccountAddress& addr, const SecretKey& viewSecretKey, std::vector<uint32_t>& out, uint64_t& amount) const {
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

  size_t TransactionImpl::getRequiredSignaturesCount(size_t index) const {
    return ::getRequiredSignaturesCount(getInputChecked(transaction, index));
  }

  bool TransactionImpl::validateInputs() const {
    return
      check_inputs_types_supported(transaction) &&
      check_inputs_overflow(transaction) &&
      checkInputsKeyimagesDiff(transaction) &&
      checkMultisignatureInputsDiff(transaction);
  }

  bool TransactionImpl::validateOutputs() const {
    return
      check_outs_valid(transaction) &&
      check_outs_overflow(transaction);
  }

  bool TransactionImpl::validateSignatures() const {
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
