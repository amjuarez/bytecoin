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
void serialize(ParentBlockSerializer& pbs, const std::string& name, ISerializer& serializer);
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
void serialize(tx_extra_merge_mining_tag& tag, const std::string& name, ISerializer& serializer);

} //namespace cryptonote
