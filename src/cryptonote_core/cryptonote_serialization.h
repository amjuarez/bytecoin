// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include "cryptonote_basic.h"
#include "serialization/ISerializer.h"

namespace crypto {

bool serialize(public_key& pubKey, Common::StringView name, CryptoNote::ISerializer& serializer);
bool serialize(secret_key& secKey, Common::StringView name, CryptoNote::ISerializer& serializer);
bool serialize(hash& h, Common::StringView name, CryptoNote::ISerializer& serializer);
bool serialize(chacha8_iv& chacha, Common::StringView name, CryptoNote::ISerializer& serializer);
bool serialize(key_image& keyImage, Common::StringView name, CryptoNote::ISerializer& serializer);
bool serialize(signature& sig, Common::StringView name, CryptoNote::ISerializer& serializer);

} //namespace crypto

namespace CryptoNote {
void serialize(ParentBlockSerializer& pbs, ISerializer& serializer);
void serialize(TransactionPrefix& txP, ISerializer& serializer);
void serialize(Transaction& tx, ISerializer& serializer);
void serialize(TransactionInput& in, ISerializer& serializer);
void serialize(TransactionOutput& in, ISerializer& serializer);

void serialize(TransactionInputGenerate& gen, ISerializer& serializer);
void serialize(TransactionInputToScript& script, ISerializer& serializer);
void serialize(TransactionInputToScriptHash& scripthash, ISerializer& serializer);
void serialize(TransactionInputToKey& key, ISerializer& serializer);
void serialize(TransactionInputMultisignature& multisignature, ISerializer& serializer);

void serialize(TransactionOutput& output, ISerializer& serializer);

void serialize(TransactionOutputTarget& output, ISerializer& serializer);

void serialize(TransactionOutputToScript& script, ISerializer& serializer);
void serialize(TransactionOutputToScriptHash& scripthash, ISerializer& serializer);
void serialize(TransactionOutputToKey& key, ISerializer& serializer);
void serialize(TransactionOutputMultisignature& multisignature, ISerializer& serializer);
void serialize(BlockHeader& header, ISerializer& serializer);
void serialize(Block& block, ISerializer& serializer);
void serialize(tx_extra_merge_mining_tag& tag, ISerializer& serializer);

void serialize(AccountPublicAddress& address, ISerializer& serializer);
void serialize(account_keys& keys, ISerializer& s);


} //namespace CryptoNote
