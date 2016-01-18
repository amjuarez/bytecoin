// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>

#include <CryptoNote.h>

namespace CryptoNote {

class NewBlockMessage {
public:
  NewBlockMessage(const Crypto::Hash& hash);
  NewBlockMessage() = default;
  void get(Crypto::Hash& hash) const;
private:
  Crypto::Hash blockHash;
};

class NewAlternativeBlockMessage {
public:
  NewAlternativeBlockMessage(const Crypto::Hash& hash);
  NewAlternativeBlockMessage() = default;
  void get(Crypto::Hash& hash) const;
private:
  Crypto::Hash blockHash;
};

class ChainSwitchMessage {
public:
  ChainSwitchMessage(std::vector<Crypto::Hash>&& hashes);
  ChainSwitchMessage(const ChainSwitchMessage& other);
  void get(std::vector<Crypto::Hash>& hashes) const;
private:
  std::vector<Crypto::Hash> blocksFromCommonRoot;
};

class BlockchainMessage {
public:
  enum class MessageType {
    NEW_BLOCK_MESSAGE,
    NEW_ALTERNATIVE_BLOCK_MESSAGE,
    CHAIN_SWITCH_MESSAGE
  };

  BlockchainMessage(NewBlockMessage&& message);
  BlockchainMessage(NewAlternativeBlockMessage&& message);
  BlockchainMessage(ChainSwitchMessage&& message);

  BlockchainMessage(const BlockchainMessage& other);

  ~BlockchainMessage();

  MessageType getType() const;

  bool getNewBlockHash(Crypto::Hash& hash) const;
  bool getNewAlternativeBlockHash(Crypto::Hash& hash) const;
  bool getChainSwitch(std::vector<Crypto::Hash>& hashes) const;
private:
  const MessageType type;

  union {
    NewBlockMessage newBlockMessage;
    NewAlternativeBlockMessage newAlternativeBlockMessage;
    ChainSwitchMessage* chainSwitchMessage;
  };
};

}
