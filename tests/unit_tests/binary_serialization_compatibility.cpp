// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

void fillBlockHeader(cryptonote::BlockHeader& header) {
  header.majorVersion = 1;
  header.minorVersion = 1;
  header.nonce = 0x807F00AB;
  header.timestamp = 1408106672;
  fillHash(header.prevId);
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

void compareBlocks(cryptonote::Block& block, cryptonote::Block& restoredBlock) {
  ASSERT_EQ(block.majorVersion, restoredBlock.majorVersion);
  ASSERT_EQ(block.minorVersion, restoredBlock.minorVersion);
  ASSERT_EQ(block.timestamp, restoredBlock.timestamp);
  ASSERT_EQ(block.prevId, restoredBlock.prevId);
  ASSERT_EQ(block.nonce, restoredBlock.nonce);
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

TEST(BinarySerializationCompatibility, Block) {
  cryptonote::Block block;
  fillBlockHeader(block);
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
