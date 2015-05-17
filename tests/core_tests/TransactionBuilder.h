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

#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/Currency.h"

class TransactionBuilder {
public:

  typedef std::vector<cryptonote::account_keys> KeysVector;
  typedef std::vector<crypto::signature> SignatureVector;
  typedef std::vector<SignatureVector> SignatureMultivector;

  struct MultisignatureSource {
    cryptonote::TransactionInputMultisignature input;
    KeysVector keys;
    crypto::public_key srcTxPubKey;
    size_t srcOutputIndex;
  };

  TransactionBuilder(const cryptonote::Currency& currency, uint64_t unlockTime = 0);

  // regenerate transaction keys
  TransactionBuilder& newTxKeys();
  TransactionBuilder& setTxKeys(const cryptonote::KeyPair& txKeys);

  // inputs
  TransactionBuilder& setInput(const std::vector<cryptonote::tx_source_entry>& sources, const cryptonote::account_keys& senderKeys);
  TransactionBuilder& addMultisignatureInput(const MultisignatureSource& source);

  // outputs
  TransactionBuilder& setOutput(const std::vector<cryptonote::tx_destination_entry>& destinations);
  TransactionBuilder& addOutput(const cryptonote::tx_destination_entry& dest);
  TransactionBuilder& addMultisignatureOut(uint64_t amount, const KeysVector& keys, uint32_t required);

  cryptonote::Transaction build() const;

  std::vector<cryptonote::tx_source_entry> m_sources;
  std::vector<cryptonote::tx_destination_entry> m_destinations;

private:

  void fillInputs(cryptonote::Transaction& tx, std::vector<cryptonote::KeyPair>& contexts) const;
  void fillOutputs(cryptonote::Transaction& tx) const;
  void signSources(const crypto::hash& prefixHash, const std::vector<cryptonote::KeyPair>& contexts, cryptonote::Transaction& tx) const;

  struct MultisignatureDestination {
    uint64_t amount;
    uint32_t requiredSignatures;
    KeysVector keys;
  };

  cryptonote::account_keys m_senderKeys;

  std::vector<MultisignatureSource> m_msigSources;
  std::vector<MultisignatureDestination> m_msigDestinations;

  size_t m_version;
  uint64_t m_unlockTime;
  cryptonote::KeyPair m_txKey;
  const cryptonote::Currency& m_currency;
};
