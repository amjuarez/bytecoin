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

#include <boost/lexical_cast.hpp>

#include "serialization/KVBinaryInputStreamSerializer.h"
#include "serialization/KVBinaryOutputStreamSerializer.h"

#include "serialization/keyvalue_serialization.h"
#include "serialization/keyvalue_serialization_overloads.h"
#include "storages/portable_storage.h"
#include "storages/portable_storage_from_bin.h"
#include "storages/portable_storage_template_helper.h"

#include <array>

using namespace cryptonote;

namespace cryptonote {


template <typename Cont> 
void serializeAsPod(Cont& cont, const std::string& name, ISerializer& s) {

  typedef typename Cont::value_type ElementType;
  const size_t elementSize = sizeof(ElementType);
  std::string buf;

  if (s.type() == ISerializer::INPUT) {
    s.binary(buf, name);
    const ElementType* ptr = reinterpret_cast<const ElementType*>(buf.data());
    size_t count = buf.size() / elementSize;
    cont.insert(cont.begin(), ptr, ptr + count);
  } else {
    auto rawSize = cont.size() * elementSize;
    auto ptr = reinterpret_cast<const char*>(cont.data());
    buf.assign(ptr, ptr + rawSize);
    s.binary(buf, name);
  }
}


struct TestElement {
  std::string name;
  uint32_t nonce;
  std::array<uint8_t, 16> blob;
  std::vector<uint32_t> u32array;

  bool operator == (const TestElement& other) const {
    return 
      name == other.name && 
      nonce == other.nonce &&
      blob == other.blob && 
      u32array == other.u32array;
  }

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(name)
    KV_SERIALIZE(nonce)
    KV_SERIALIZE_VAL_POD_AS_BLOB(blob)
    KV_SERIALIZE_CONTAINER_POD_AS_BLOB(u32array)
  END_KV_SERIALIZE_MAP()


  void serialize(ISerializer& s, const std::string& nm) {
    s.beginObject(nm);
    s(name, "name");
    s(nonce, "nonce");
    s.binary(blob.data(), blob.size(), "blob");
    serializeAsPod(u32array, "u32array", s);
    s.endObject();
  }
};

struct TestStruct {
  uint8_t u8;
  uint32_t u32;
  uint64_t u64;
  std::vector<TestElement> vec1;
  std::vector<TestElement> vec2;
  TestElement root;

  bool operator == (const TestStruct& other) const {
    return
      root == other.root &&
      u8 == other.u8 &&
      u32 == other.u32 &&
      u64 == other.u64 &&
      vec1 == other.vec1 &&
      vec2 == other.vec2;
  }

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(root)
    KV_SERIALIZE(vec1)
    KV_SERIALIZE(vec2)
    KV_SERIALIZE(u8)
    KV_SERIALIZE(u32)
    KV_SERIALIZE(u64)
  END_KV_SERIALIZE_MAP()

  void serialize(ISerializer& s, const std::string& name) {
    s.beginObject(name);
    s(root, "root");
    s(vec1, "vec1");
    s(vec2, "vec2");
    s(u8, "u8");
    s(u32, "u32");
    s(u64, "u64");
    s.endObject();
  }

};

}


#include <chrono>

typedef std::chrono::high_resolution_clock hclock;

class HiResTimer {
public:
  HiResTimer() : 
    start(hclock::now()) {}

  std::chrono::duration<double> duration() {
    return hclock::now() - start;
  }

private:
  hclock::time_point start;
};

TEST(KVSerialize, Simple) {
  TestElement testData1, testData2;
  std::string buf;

  testData1.name = "hello";
  testData1.nonce = 12345;
  testData1.u32array.resize(128);

  testData2.name = "bye";
  testData2.nonce = 54321;

  epee::serialization::store_t_to_binary(testData1, buf);

  std::stringstream s(buf);
  KVBinaryInputStreamSerializer kvInput(s);
  kvInput.parse();
  kvInput(testData2, "");

  EXPECT_EQ(testData1, testData2);
}


TEST(KVSerialize, NewWriterOldReader) {
  std::string bufOld, bufNew;
  TestStruct s1;
  TestStruct s2;

  s1.u64 = 0xffULL << 50;
  s1.vec1.resize(37);
  s1.root.name = "somename";
  s1.root.u32array.resize(128);

  s2.u64 = 13;
  s2.vec2.resize(10);

  {
    HiResTimer t;
    epee::serialization::store_t_to_binary(s1, bufOld);
    std::cout << "Old serialization: " << t.duration().count() << std::endl;
  }
  
  {
    HiResTimer t;

    KVBinaryOutputStreamSerializer kvOut;
    kvOut(s1, "");
    std::stringstream out;
    kvOut.write(out);
    bufNew = out.str();

    std::cout << "New serialization: " << t.duration().count() << std::endl;
  }

  {
    HiResTimer t;
    TestStruct outStruct(s2);

    std::stringstream s(bufOld);
    KVBinaryInputStreamSerializer kvInput(s);
    kvInput.parse();
    kvInput(outStruct, "");

    std::cout << "New deserialization: " << t.duration().count() << std::endl;

    EXPECT_EQ(s1, outStruct);
  }


  {
    HiResTimer t;
    TestStruct outStruct(s2);

    bool parseOld = epee::serialization::load_t_from_binary(outStruct, bufOld);

    ASSERT_TRUE(parseOld);

    std::cout << "Old deserialization: " << t.duration().count() << std::endl;

    EXPECT_EQ(s1, outStruct);
  }

  {
    TestStruct outStruct(s2);
    bool parseNew = epee::serialization::load_t_from_binary(outStruct, bufNew);
    ASSERT_TRUE(parseNew);
    EXPECT_EQ(s1, outStruct);
  }


}
