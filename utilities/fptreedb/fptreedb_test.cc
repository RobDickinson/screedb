/*
 * Copyright 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Unit tests for RocksDB-style database with "Fingerprinting Persistent Tree" and NVML backend.
// See utilities/fptreedb/fptreedb.cc for implementation.

#include "fptreedb.h"
#include "gtest/gtest.h"

using namespace rocksdb;

std::string kDBPath = "/tmp/fptreedb_test";

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class FPTreeDBTest : public testing::Test {
public:
  FPTreeDB* db;

  FPTreeDBTest() {
    std::remove(kDBPath.c_str());
    Options options;
    options.create_if_missing = true;  // todo option is ignored, see #7
    FPTreeDBOptions fptree_options;
    Status s = FPTreeDB::Open(options, fptree_options, kDBPath, &db);
    assert(s.ok() && db->GetName() == kDBPath);
  }

  ~FPTreeDBTest() { delete db; }
};

TEST_F(FPTreeDBTest, DeleteExistingTest) {
  Status s = db->Put(WriteOptions(), "tmpkey", "tmpvalue1");
  ASSERT_TRUE(s.ok());
  s = db->Delete(WriteOptions(), "tmpkey");
  ASSERT_TRUE(s.ok());
  std::string value;
  s = db->Get(ReadOptions(), "tmpkey", &value);
  ASSERT_TRUE(s.IsNotFound());
  s = db->Delete(WriteOptions(), "tmpkey");  // no harm in deleting twice
  ASSERT_TRUE(s.ok());
}

TEST_F(FPTreeDBTest, DeleteHeadlessTest) {
  Status s = db->Delete(WriteOptions(), "nada");
  ASSERT_TRUE(s.ok());
}

TEST_F(FPTreeDBTest, DeleteNonexistentTest) {
  Status s = db->Put(WriteOptions(), "key1", "value1");
  ASSERT_TRUE(s.ok());
  s = db->Delete(WriteOptions(), "nada");
  ASSERT_TRUE(s.ok());
}

TEST_F(FPTreeDBTest, GetHeadlessTest) {
  std::string value;
  Status s = db->Get(ReadOptions(), "waldo", &value);
  ASSERT_TRUE(s.IsNotFound());
}

TEST_F(FPTreeDBTest, GetNonexistentTest) {
  Status s = db->Put(WriteOptions(), "key1", "value1");
  ASSERT_TRUE(s.ok());
  std::string value;
  s = db->Get(ReadOptions(), "waldo", &value);
  ASSERT_TRUE(s.IsNotFound());
}

TEST_F(FPTreeDBTest, MergeTest) {
  Status s = db->Merge(WriteOptions(), "key1", "value1");
  ASSERT_TRUE(s.ok());
  std::string value;
  s = db->Get(ReadOptions(), "key1", &value);
  ASSERT_TRUE(s.ok() && value == "value1");
}

TEST_F(FPTreeDBTest, MultiGetTest) {
  Status s = db->Put(WriteOptions(), "tmpkey", "tmpvalue1");
  ASSERT_TRUE(s.ok());
  s = db->Put(WriteOptions(), "tmpkey2", "tmpvalue2");
  ASSERT_TRUE(s.ok());
  std::vector<std::string> values = std::vector<std::string>();
  std::vector<Slice> keys = std::vector<Slice>();
  keys.push_back("tmpkey");
  keys.push_back("tmpkey2");
  keys.push_back("tmpkey3");
  keys.push_back("tmpkey");
  std::vector<Status> status = db->MultiGet(ReadOptions(), keys, &values);
  ASSERT_TRUE(status.size() == 4);
  ASSERT_TRUE(values.size() == 4);
  ASSERT_TRUE(status.at(0).ok() && values.at(0) == "tmpvalue1");
  ASSERT_TRUE(status.at(1).ok() && values.at(1) == "tmpvalue2");
  ASSERT_TRUE(status.at(2).IsNotFound() && values.at(2) == "");
  ASSERT_TRUE(status.at(3).ok() && values.at(3) == "tmpvalue1");
}

TEST_F(FPTreeDBTest, PutTest) {
  Status s = db->Put(WriteOptions(), "key1", "value1");
  ASSERT_TRUE(s.ok());
  std::string value;
  s = db->Get(ReadOptions(), "key1", &value);
  ASSERT_TRUE(s.ok() && value == "value1");
}

TEST_F(FPTreeDBTest, PutExistingTest) {
  Status s = db->Put(WriteOptions(), "key1", "value1");
  ASSERT_TRUE(s.ok());
  std::string value;
  s = db->Get(ReadOptions(), "key1", &value);
  ASSERT_TRUE(s.ok() && value == "value1");
  s = db->Put(WriteOptions(), "key1", "value_replaced");
  ASSERT_TRUE(s.ok());
  std::string new_value;
  s = db->Get(ReadOptions(), "key1", &new_value);
  ASSERT_TRUE(s.ok() && new_value == "value_replaced");
}

std::string too_big = std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQRSTUVW");

TEST_F(FPTreeDBTest, PutTruncatedKeyTest) {
  Status s = db->Put(WriteOptions(), too_big, "value2");
  ASSERT_TRUE(s.ok());
  std::string value;
  s = db->Get(ReadOptions(), too_big, &value);
  ASSERT_TRUE(s.IsNotFound()); // because the key was truncated!
  s = db->Get(ReadOptions(), too_big.substr(0, KEY_LENGTH), &value);
  ASSERT_TRUE(s.ok() && value == "value2");
}

TEST_F(FPTreeDBTest, PutTruncatedValueTest) {
  Status s = db->Put(WriteOptions(), "key3", too_big);
  ASSERT_TRUE(s.ok());
  std::string value;
  s = db->Get(ReadOptions(), "key3", &value);
  ASSERT_TRUE(s.ok() && value == too_big.substr(0, VALUE_LENGTH));
}

TEST_F(FPTreeDBTest, WriteTest) {
  WriteBatch batch;
  batch.Delete("key1");
  batch.Put("key2", "value2");
  Status s = db->Write(WriteOptions(), &batch);
  ASSERT_TRUE(s.IsNotSupported());
  // ASSERT_TRUE(s.ok());
  // std::string value;
  // s = db->Get(ReadOptions(), "key1", &value);
  // ASSERT_TRUE(s.IsNotFound());
  // db->Get(ReadOptions(), "key2", &value);
  // ASSERT_TRUE(value == "value");
}
