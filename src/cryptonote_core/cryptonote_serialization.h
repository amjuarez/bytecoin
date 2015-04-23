// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_basic.h"

namespace cryptonote {
class ISerializer;
}

namespace crypto {

void serialize(public_key& pubKey, const std::string& name, cryptonote::ISerializer& enumerator);
void serialize(secret_key& secKey, const std::string& name, cryptonote::ISerializer& enumerator);
void serialize(hash& h, const std::string& name, cryptonote::ISerializer& enumerator);
void serialize(chacha8_iv& chacha, const std::string& name, cryptonote::ISerializer& enumerator);
void serialize(key_image& keyImage, const std::string& name, cryptonote::ISerializer& enumerator);

} //namespace crypto

namespace cryptonote {
void serialize(TransactionPrefix& txP, const std::string& name, ISerializer& serializer);
void serialize(Transaction& tx, const std::string& name, ISerializer& serializer);
void serialize(TransactionInput& in, const std::string& name, ISerializer& serializer);
void serialize(TransactionOutput& in, const std::string& name, ISerializer& serializer);

void serialize(TransactionInputGenerate& gen, const std::string& name, ISerializer& serializer);
void serialize(TransactionInputToScript& script, const std::string& name, ISerializer& serializer);
void serialize(TransactionInputToScriptHash& scripthash, const std::string& name, ISerializer& serializer);
void serialize(TransactionInputToKey& key, const std::string& name, ISerializer& serializer);
void serialize(TransactionInputMultisignature& multisignature, const std::string& name, ISerializer& serializer);

void serialize(TransactionOutput& output, const std::string& name, ISerializer& serializer);

void serialize(TransactionOutputTarget& output, const std::string& name, ISerializer& serializer);

void serialize(TransactionOutputToScript& script, const std::string& name, ISerializer& serializer);
void serialize(TransactionOutputToScriptHash& scripthash, const std::string& name, ISerializer& serializer);
void serialize(TransactionOutputToKey& key, const std::string& name, ISerializer& serializer);
void serialize(TransactionOutputMultisignature& multisignature, const std::string& name, ISerializer& serializer);
void serialize(BlockHeader& header, const std::string& name, ISerializer& serializer);
void serialize(Block& block, const std::string& name, ISerializer& serializer);
void serialize(AccountPublicAddress& address, const std::string& name, ISerializer& serializer);

} //namespace cryptonote
