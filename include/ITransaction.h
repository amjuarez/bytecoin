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

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace CryptoNote {

typedef std::array<uint8_t, 32> PublicKey;
typedef std::array<uint8_t, 32> SecretKey;
typedef std::array<uint8_t, 32> KeyImage;
typedef std::array<uint8_t, 32> Hash;
typedef std::vector<uint8_t> Blob;

struct AccountAddress {
  PublicKey spendPublicKey;
  PublicKey viewPublicKey;
};

struct AccountKeys {
  AccountAddress address;
  SecretKey spendSecretKey;
  SecretKey viewSecretKey;
};

struct KeyPair {
  PublicKey publicKey;
  SecretKey secretKey;
};

namespace TransactionTypes {
  
  enum class InputType : uint8_t { Invalid, Key, Multisignature, Generating };
  enum class OutputType : uint8_t { Invalid, Key, Multisignature };

  struct InputKey {
    uint64_t amount;
    std::vector<uint64_t> keyOffsets;
    KeyImage keyImage;      // double spending protection
  };

  struct InputMultisignature {
    uint64_t amount;
    uint32_t signatures;
    uint64_t outputIndex;
  };

  struct OutputKey {
    uint64_t amount;
    PublicKey key;
  };

  struct OutputMultisignature {
    uint64_t amount;
    std::vector<PublicKey> keys;
    uint32_t requiredSignatures;
  };

  struct GlobalOutput {
    PublicKey targetKey;
    uint64_t outputIndex;
  };

  typedef std::vector<GlobalOutput> GlobalOutputsContainer;

  struct OutputKeyInfo {
    PublicKey transactionPublicKey;
    size_t transactionIndex;
    size_t outputInTransaction;
  };

  struct InputKeyInfo {
    uint64_t amount;
    GlobalOutputsContainer outputs;
    OutputKeyInfo realOutput;
  };

}

//
// ITransactionReader
// 
class ITransactionReader {
public:
  virtual ~ITransactionReader() { }

  virtual Hash getTransactionHash() const = 0;
  virtual Hash getTransactionPrefixHash() const = 0;
  virtual PublicKey getTransactionPublicKey() const = 0;
  virtual uint64_t getUnlockTime() const = 0;

  // extra
  virtual bool getPaymentId(Hash& paymentId) const = 0;
  virtual bool getExtraNonce(std::string& nonce) const = 0;

  // inputs
  virtual size_t getInputCount() const = 0;
  virtual uint64_t getInputTotalAmount() const = 0;
  virtual TransactionTypes::InputType getInputType(size_t index) const = 0;
  virtual void getInput(size_t index, TransactionTypes::InputKey& input) const = 0;
  virtual void getInput(size_t index, TransactionTypes::InputMultisignature& input) const = 0;

  // outputs
  virtual size_t getOutputCount() const = 0;
  virtual uint64_t getOutputTotalAmount() const = 0;
  virtual TransactionTypes::OutputType getOutputType(size_t index) const = 0;
  virtual void getOutput(size_t index, TransactionTypes::OutputKey& output) const = 0;
  virtual void getOutput(size_t index, TransactionTypes::OutputMultisignature& output) const = 0;

  // signatures
  virtual size_t getRequiredSignaturesCount(size_t inputIndex) const = 0;
  virtual bool findOutputsToAccount(const AccountAddress& addr, const SecretKey& viewSecretKey, std::vector<uint32_t>& outs, uint64_t& outputAmount) const = 0;

  // various checks
  virtual bool validateInputs() const = 0;
  virtual bool validateOutputs() const = 0;
  virtual bool validateSignatures() const = 0;

  // serialized transaction
  virtual Blob getTransactionData() const = 0;
};

//
// ITransactionWriter
// 
class ITransactionWriter {
public: 

  virtual ~ITransactionWriter() { }

  // transaction parameters
  virtual void setUnlockTime(uint64_t unlockTime) = 0;

  // extra
  virtual void setPaymentId(const Hash& paymentId) = 0;
  virtual void setExtraNonce(const std::string& nonce) = 0;

  // Inputs/Outputs 
  virtual size_t addInput(const TransactionTypes::InputKey& input) = 0;
  virtual size_t addInput(const AccountKeys& senderKeys, const TransactionTypes::InputKeyInfo& info, KeyPair& ephKeys) = 0;
  virtual size_t addInput(const TransactionTypes::InputMultisignature& input) = 0;

  virtual size_t addOutput(uint64_t amount, const AccountAddress& to) = 0;
  virtual size_t addOutput(uint64_t amount, const std::vector<AccountAddress>& to, uint32_t requiredSignatures) = 0;

  // transaction info
  virtual bool getTransactionSecretKey(SecretKey& key) const = 0;
  virtual void setTransactionSecretKey(const SecretKey& key) = 0;

  // signing
  virtual void signInputKey(size_t input, const TransactionTypes::InputKeyInfo& info, const KeyPair& ephKeys) = 0;
  virtual void signInputMultisignature(size_t input, const PublicKey& sourceTransactionKey, size_t outputIndex, const AccountKeys& accountKeys) = 0;
};

class ITransaction : 
  public ITransactionReader, 
  public ITransactionWriter {
public:
  virtual ~ITransaction() { }

};

}
