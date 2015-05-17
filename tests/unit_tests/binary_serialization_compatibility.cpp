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


#include "gtest/gtest.h"

#include <sstream>
#include <limits>
#include <boost/variant/get.hpp>

#include "serialization/BinaryOutputStreamSerializer.h"
#include "serialization/BinaryInputStreamSerializer.h"
#include "serialization/serialization.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/cryptonote_serialization.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "serialization_structs_comparators.h"

#include <iostream>
#include "string_tools.h"

template <typename Struct>
void checkEqualBinary(Struct& original) {
  std::stringstream newStream;
  std::stringstream oldStream;

  cryptonote::BinaryOutputStreamSerializer binarySerializer(newStream);
  binarySerializer(original, "");

  binary_archive<true> ba(oldStream);
  bool r = ::serialization::serialize(ba, original);
  ASSERT_TRUE(r);

  ASSERT_EQ(oldStream.str(), newStream.str());
}

template <typename Struct>
void checkEnumeratorToLegacy(Struct& original) {
  std::stringstream archive;

  cryptonote::BinaryOutputStreamSerializer binarySerializer(archive);
  binarySerializer(original, "");

  //std::cout << "enumerated string: " << epee::string_tools::buff_to_hex_nodelimer(archive.str()) << std::endl;

  Struct restored;
  binary_archive<false> ba(archive);
  bool r = ::serialization::serialize(ba, restored);
  //ASSERT_TRUE(r);

  ASSERT_EQ(original, restored);
}

template <typename Struct>
void checkLegacyToEnumerator(Struct& original) {
  std::stringstream archive;

  binary_archive<true> ba(archive);
  bool r = ::serialization::serialize(ba, original);
  ASSERT_TRUE(r);

  //std::cout << "legacy string: " << epee::string_tools::buff_to_hex_nodelimer(archive.str()) << std::endl;

  Struct restored;

  cryptonote::BinaryInputStreamSerializer binarySerializer(archive);
  binarySerializer(restored, "");

  ASSERT_EQ(original, restored);
}

template <typename Struct>
void checkEnumeratorToEnumerator(Struct& original) {
  std::stringstream archive;

  cryptonote::BinaryOutputStreamSerializer output(archive);
  output(original, "");

  Struct restored;
  cryptonote::BinaryInputStreamSerializer input(archive);
  input(restored, "");

  ASSERT_EQ(original, restored);
}

template <typename Struct>
void checkCompatibility(Struct& original) {
  checkEqualBinary(original);
  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToEnumerator(original));
  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToLegacy(original));
  ASSERT_NO_FATAL_FAILURE(checkLegacyToEnumerator(original));
}

void fillData(char* data, size_t size, char startByte) {
  for (size_t i = 0; i < size; ++i) {
    data[i] = startByte++;
  }
}

void fillPublicKey(crypto::public_key& key, char startByte = 120) {
  fillData(reinterpret_cast<char *>(&key), sizeof(crypto::public_key), startByte);
}

void fillHash(crypto::hash& hash, char startByte = 120) {
  fillData(reinterpret_cast<char *>(&hash), sizeof(crypto::hash), startByte);
}

void fillKeyImage(crypto::key_image& image, char startByte = 120) {
  fillData(reinterpret_cast<char *>(&image), sizeof(crypto::key_image), startByte);
}

void fillSignature(crypto::signature& sig, char startByte = 120) {
  fillData(reinterpret_cast<char *>(&sig), sizeof(crypto::signature), startByte);
}

void fillTransactionOutputMultisignature(cryptonote::TransactionOutputMultisignature& s) {
  crypto::public_key key;
  fillPublicKey(key, 0);
  s.keys.push_back(key);

  char start = 120;

  fillPublicKey(key, start++);
  s.keys.push_back(key);

  fillPublicKey(key, start++);
  s.keys.push_back(key);

  fillPublicKey(key, start++);
  s.keys.push_back(key);

  fillPublicKey(key, start++);
  s.keys.push_back(key);

  fillPublicKey(key, start++);
  s.keys.push_back(key);

  s.requiredSignatures = 12;
}

void fillTransaction(cryptonote::Transaction& tx) {
  tx.version = 1;
  tx.unlockTime = 0x7f1234560089ABCD;

  cryptonote::TransactionInputGenerate gen;
  gen.height = 0xABCD123456EF;
  tx.vin.push_back(gen);

  cryptonote::TransactionInputToKey key;
  key.amount = 500123;
  key.keyOffsets = {12,3323,0x7f0000000000, std::numeric_limits<uint64_t>::max(), 0};
  fillKeyImage(key.keyImage);
  tx.vin.push_back(key);

  cryptonote::TransactionInputMultisignature multisig;
  multisig.amount = 490000000;
  multisig.outputIndex = 424242;
  multisig.signatures = 4;
  tx.vin.push_back(multisig);

  cryptonote::TransactionOutput txOutput;
  txOutput.amount = 0xfff000ffff778822;
  cryptonote::TransactionOutputToKey out;
  fillPublicKey(out.key);
  txOutput.target = out;
  tx.vout.push_back(txOutput);

  tx.extra = {1,2,3,127,0,128,255};

  tx.signatures.resize(3);

  for (size_t i = 0; i < boost::get<cryptonote::TransactionInputToKey>(tx.vin[1]).keyOffsets.size(); ++i) {
    crypto::signature sig;
    fillSignature(sig, i);
    tx.signatures[1].push_back(sig);
  }

  for (size_t i = 0; i < boost::get<cryptonote::TransactionInputMultisignature>(tx.vin[2]).signatures; ++i) {
    crypto::signature sig;
    fillSignature(sig, i+120);
    tx.signatures[2].push_back(sig);
  }
}

void fillParentBlock(cryptonote::ParentBlock& pb) {
  pb.majorVersion = 1;
  pb.minorVersion = 1;

  fillHash(pb.prevId, 120);

  pb.numberOfTransactions = 3;
  size_t branchSize = crypto::tree_depth(pb.numberOfTransactions);
  for (size_t i = 0; i < branchSize; ++i) {
    crypto::hash hash;
    fillHash(hash, i);
    pb.minerTxBranch.push_back(hash);
  }

  fillTransaction(pb.minerTx);

  cryptonote::tx_extra_merge_mining_tag mmTag;
  mmTag.depth = 10;
  fillHash(mmTag.merkle_root);
  pb.minerTx.extra.clear();
  cryptonote::append_mm_tag_to_extra(pb.minerTx.extra, mmTag);

  std::string my;
  std::copy(pb.minerTx.extra.begin(), pb.minerTx.extra.end(), std::back_inserter(my));

  for (size_t i = 0; i < mmTag.depth; ++i) {
    crypto::hash hash;
    fillHash(hash, i);
    pb.blockchainBranch.push_back(hash);
  }
}

void fillBlockHeaderVersion1(cryptonote::BlockHeader& header) {
  header.majorVersion = 1;
  header.minorVersion = 1;
  header.nonce = 0x807F00AB;
  header.timestamp = 1408106672;
  fillHash(header.prevId);
}

void fillBlockHeaderVersion2(cryptonote::BlockHeader& header) {
  fillBlockHeaderVersion1(header);
  header.majorVersion = 2;
}

TEST(BinarySerializationCompatibility, TransactionOutputMultisignature) {
  cryptonote::TransactionOutputMultisignature s;

  fillTransactionOutputMultisignature(s);

  checkCompatibility(s);
}

TEST(BinarySerializationCompatibility, TransactionInputGenerate) {
  cryptonote::TransactionInputGenerate s;
  s.height = 0x8000000000000001;
  checkCompatibility(s);

  s.height = 0x7FFFFFFFFFFFFFFF;
  checkCompatibility(s);

  s.height = 0;
  checkCompatibility(s);
};

TEST(BinarySerializationCompatibility, TransactionInputToKey) {
  cryptonote::TransactionInputToKey s;

  s.amount = 123456987032;
  s.keyOffsets = {12,3323,0x7f00000000000000, std::numeric_limits<uint64_t>::max(), 0};
  fillKeyImage(s.keyImage);

  checkCompatibility(s);
}

TEST(BinarySerializationCompatibility, TransactionInputMultisignature) {
  cryptonote::TransactionInputMultisignature s;
  s.amount = 0xfff000ffff778822;
  s.signatures = 0x7f259200;
  s.outputIndex = 0;

  checkCompatibility(s);
}

TEST(BinarySerializationCompatibility, TransactionOutput_TransactionOutputToKey) {
  cryptonote::TransactionOutput s;
  s.amount = 0xfff000ffff778822;

  cryptonote::TransactionOutputToKey out;
  fillPublicKey(out.key);
  s.target = out;

  checkCompatibility(s);
}

TEST(BinarySerializationCompatibility, TransactionOutput_TransactionOutputMultisignature) {
  cryptonote::TransactionOutput s;
  s.amount = 0xfff000ffff778822;

  cryptonote::TransactionOutputMultisignature out;
  fillTransactionOutputMultisignature(out);
  s.target = out;

  checkCompatibility(s);
}

TEST(BinarySerializationCompatibility, Transaction) {
  cryptonote::Transaction tx;

  fillTransaction(tx);

  checkCompatibility(tx);
}

void compareParentBlocks(cryptonote::ParentBlock& pb, cryptonote::ParentBlock& restoredPb, bool headerOnly) {
  EXPECT_EQ(pb.majorVersion, restoredPb.majorVersion);
  EXPECT_EQ(pb.minorVersion, restoredPb.minorVersion);
  EXPECT_EQ(pb.prevId, restoredPb.prevId);

  if (headerOnly) {
    return;
  }

  EXPECT_EQ(pb.numberOfTransactions, restoredPb.numberOfTransactions);
  EXPECT_EQ(pb.minerTxBranch, restoredPb.minerTxBranch);
  EXPECT_EQ(pb.minerTx, restoredPb.minerTx);
  EXPECT_EQ(pb.blockchainBranch, restoredPb.blockchainBranch);
}

void checkEnumeratorToLegacy(cryptonote::ParentBlock& pb, uint64_t ts, uint32_t nonce, bool hashingSerialization, bool headerOnly) {
  std::stringstream archive;

  cryptonote::ParentBlockSerializer original(pb, ts, nonce, hashingSerialization, headerOnly);
  cryptonote::BinaryOutputStreamSerializer output(archive);
  output(original, "");

  cryptonote::ParentBlock restoredPb;
  uint64_t restoredTs;
  uint32_t restoredNonce;

  cryptonote::ParentBlockSerializer restored(restoredPb, restoredTs, restoredNonce, hashingSerialization, headerOnly);
  binary_archive<false> ba(archive);
  bool r = ::serialization::serialize(ba, restored);
  ASSERT_TRUE(r);

  EXPECT_EQ(nonce, restoredNonce);
  EXPECT_EQ(ts, restoredTs);

  ASSERT_NO_FATAL_FAILURE(compareParentBlocks(pb, restoredPb, headerOnly));
}

void checkLegacyToEnumerator(cryptonote::ParentBlock& pb, uint64_t ts, uint32_t nonce, bool hashingSerialization, bool headerOnly) {
  std::stringstream archive;

  cryptonote::ParentBlockSerializer original(pb, ts, nonce, hashingSerialization, headerOnly);
  binary_archive<true> ba(archive);
  bool r = ::serialization::serialize(ba, original);
  ASSERT_TRUE(r);

  cryptonote::ParentBlock restoredPb;
  uint64_t restoredTs;
  uint32_t restoredNonce;

  cryptonote::ParentBlockSerializer restored(restoredPb, restoredTs, restoredNonce, hashingSerialization, headerOnly);

  cryptonote::BinaryInputStreamSerializer input(archive);
  input(restored, "");

  EXPECT_EQ(nonce, restoredNonce);
  EXPECT_EQ(ts, restoredTs);

  ASSERT_NO_FATAL_FAILURE(compareParentBlocks(pb, restoredPb, headerOnly));
}

void checkEnumeratorToEnumerator(cryptonote::ParentBlock& pb, uint64_t ts, uint32_t nonce, bool hashingSerialization, bool headerOnly) {
  std::stringstream archive;

  cryptonote::ParentBlockSerializer original(pb, ts, nonce, hashingSerialization, headerOnly);
  cryptonote::BinaryOutputStreamSerializer output(archive);
  output(original, "");

  cryptonote::ParentBlock restoredPb;
  uint64_t restoredTs;
  uint32_t restoredNonce;

  cryptonote::ParentBlockSerializer restored(restoredPb, restoredTs, restoredNonce, hashingSerialization, headerOnly);

  cryptonote::BinaryInputStreamSerializer input(archive);
  input(restored, "");

  EXPECT_EQ(nonce, restoredNonce);
  EXPECT_EQ(ts, restoredTs);

  ASSERT_NO_FATAL_FAILURE(compareParentBlocks(pb, restoredPb, headerOnly));
}

void checkCompatibility(cryptonote::ParentBlock& pb, uint64_t ts, uint32_t nonce, bool hashingSerialization, bool headerOnly) {
  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToEnumerator(pb, ts, nonce, hashingSerialization, headerOnly));
  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToLegacy(pb, ts, nonce, hashingSerialization, headerOnly));
  ASSERT_NO_FATAL_FAILURE(checkLegacyToEnumerator(pb, ts, nonce, hashingSerialization, headerOnly));
}

TEST(BinarySerializationCompatibility, ParentBlockSerializer) {
  cryptonote::ParentBlock pb;
  fillParentBlock(pb);
  uint64_t timestamp = 1408106672;
  uint32_t nonce = 1234567;

  checkCompatibility(pb, timestamp, nonce, false, false);
  checkCompatibility(pb, timestamp, nonce, true, false);
  checkCompatibility(pb, timestamp, nonce, false, true);
}

void compareBlocks(cryptonote::Block& block, cryptonote::Block& restoredBlock) {
  ASSERT_EQ(block.majorVersion, restoredBlock.majorVersion);
  ASSERT_EQ(block.minorVersion, restoredBlock.minorVersion);
  if (block.majorVersion == cryptonote::BLOCK_MAJOR_VERSION_1) {
    ASSERT_EQ(block.timestamp, restoredBlock.timestamp);
    ASSERT_EQ(block.prevId, restoredBlock.prevId);
    ASSERT_EQ(block.nonce, restoredBlock.nonce);
  } else if (block.majorVersion == cryptonote::BLOCK_MAJOR_VERSION_2) {
    ASSERT_EQ(block.prevId, restoredBlock.prevId);
    ASSERT_NO_FATAL_FAILURE(compareParentBlocks(block.parentBlock, restoredBlock.parentBlock, false));
  } else {
    throw std::runtime_error("Unknown major block version. Check your test");
  }
  ASSERT_EQ(block.minerTx, restoredBlock.minerTx);
  ASSERT_EQ(block.txHashes, restoredBlock.txHashes);
}

void checkEnumeratorToLegacy(cryptonote::Block& block) {
  std::stringstream archive;

  cryptonote::BinaryOutputStreamSerializer output(archive);
  output(block, "");

  cryptonote::Block restoredBlock;

  binary_archive<false> ba(archive);
  bool r = ::serialization::serialize(ba, restoredBlock);
  ASSERT_TRUE(r);

  ASSERT_NO_FATAL_FAILURE(compareBlocks(block, restoredBlock));
}

void checkLegacyToEnumerator(cryptonote::Block& block) {
  std::stringstream archive;

  binary_archive<true> ba(archive);
  bool r = ::serialization::serialize(ba, block);
  ASSERT_TRUE(r);

  cryptonote::Block restoredBlock;

  cryptonote::BinaryInputStreamSerializer output(archive);
  output(restoredBlock, "");

  ASSERT_NO_FATAL_FAILURE(compareBlocks(block, restoredBlock));
}

void checkEnumeratorToEnumerator(cryptonote::Block& block) {
  std::stringstream archive;

  cryptonote::BinaryOutputStreamSerializer output(archive);
  output(block, "");

  cryptonote::Block restoredBlock;

//  std::cout << "enumerated string: " << epee::string_tools::buff_to_hex_nodelimer(archive.str()) << std::endl;

  cryptonote::BinaryInputStreamSerializer input(archive);
  input(restoredBlock, "");

  ASSERT_NO_FATAL_FAILURE(compareBlocks(block, restoredBlock));
}

void checkCompatibility(cryptonote::Block& block) {
  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToEnumerator(block));
  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToLegacy(block));
  ASSERT_NO_FATAL_FAILURE(checkLegacyToEnumerator(block));
}

TEST(BinarySerializationCompatibility, BlockVersion1) {
  cryptonote::Block block;
  fillBlockHeaderVersion1(block);
  fillParentBlock(block.parentBlock);
  fillTransaction(block.minerTx);

  for (size_t i = 0; i < 7; ++i) {
    crypto::hash hash;
    fillHash(hash, 0x7F + i);
    block.txHashes.push_back(hash);
  }

  checkCompatibility(block);
}

TEST(BinarySerializationCompatibility, BlockVersion2) {
  cryptonote::Block block;
  fillBlockHeaderVersion2(block);
  fillParentBlock(block.parentBlock);
  fillTransaction(block.minerTx);

  for (size_t i = 0; i < 7; ++i) {
    crypto::hash hash;
    fillHash(hash, 0x7F + i);
    block.txHashes.push_back(hash);
  }

  checkCompatibility(block);
}

TEST(BinarySerializationCompatibility, account_public_address) {
  cryptonote::AccountPublicAddress addr;

  fillPublicKey(addr.m_spendPublicKey, 0x50);
  fillPublicKey(addr.m_viewPublicKey, 0xAA);

  checkCompatibility(addr);
}

TEST(BinarySerializationCompatibility, tx_extra_merge_mining_tag) {
  cryptonote::tx_extra_merge_mining_tag tag;
  tag.depth = 0xdeadbeef;
  fillHash(tag.merkle_root);

  checkCompatibility(tag);
}

TEST(BinarySerializationCompatibility, readFromEmptyStream) {
  cryptonote::TransactionOutput t;
  std::stringstream emptyStream;
  cryptonote::BinaryInputStreamSerializer s(emptyStream);

  ASSERT_ANY_THROW(s(t, ""));
}

TEST(BinarySerializationCompatibility, writeToBadStream) {
  cryptonote::TransactionOutput t;
  std::stringstream badStream;
  cryptonote::BinaryOutputStreamSerializer s(badStream);

  badStream.setstate(std::ios::badbit);
  ASSERT_ANY_THROW(s(t, ""));
}
