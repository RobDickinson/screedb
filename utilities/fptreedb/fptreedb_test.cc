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
using namespace rocksdb::fptreedb;

std::string kDBPath = "/dev/shm/fptreedb_test";

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class FPTreeDBTest : public testing::Test {
public:
  FPTreeDB* db;

  FPTreeDBTest() {
    std::remove(kDBPath.c_str());
    Open();
  }

  ~FPTreeDBTest() { delete db; }

  void Reopen() {
    delete db;
    Open();
  }

private:
  void Open() {
    Options options;
    options.create_if_missing = true;  // todo options are ignored, see #7
    FPTreeDBOptions fptree_options;
    Status s = FPTreeDB::Open(options, fptree_options, kDBPath, &db);
    assert(s.ok() && db->GetName() == kDBPath);
  }
};

// =============================================================================================
// SINGLE-OPEN TESTS
// =============================================================================================

TEST_F(FPTreeDBTest, DeleteAllTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "tmpkey", "tmpvalue1").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "tmpkey").ok());
  // @todo additional verification for newly empty db
  ASSERT_TRUE(db->Put(WriteOptions(), "tmpkey1", "tmpvalue1").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "tmpkey1", &value).ok() && value == "tmpvalue1");
}

TEST_F(FPTreeDBTest, DeleteExistingTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "tmpkey1", "tmpvalue1").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "tmpkey2", "tmpvalue2").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "tmpkey1").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "tmpkey1").ok()); // ok to delete twice
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "tmpkey1", &value).IsNotFound());
  ASSERT_TRUE(db->Get(ReadOptions(), "tmpkey2", &value).ok() && value == "tmpvalue2");
}

TEST_F(FPTreeDBTest, DeleteHeadlessTest) {
  ASSERT_TRUE(db->Delete(WriteOptions(), "nada").ok());
}

TEST_F(FPTreeDBTest, DeleteNonexistentTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "nada").ok());
}

TEST_F(FPTreeDBTest, GetHeadlessTest) {
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "waldo", &value).IsNotFound());
}

TEST_F(FPTreeDBTest, GetNonexistentTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "waldo", &value).IsNotFound());
}

TEST_F(FPTreeDBTest, GetOneOfManyTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "abc", "A1").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "def", "B2").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "hij", "C3").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "jkl", "D4").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "mno", "E5").ok());
  std::string value1;
  ASSERT_TRUE(db->Get(ReadOptions(), "abc", &value1).ok() && value1 == "A1");
  std::string value2;
  ASSERT_TRUE(db->Get(ReadOptions(), "def", &value2).ok() && value2 == "B2");
  std::string value3;
  ASSERT_TRUE(db->Get(ReadOptions(), "hij", &value3).ok() && value3 == "C3");
  std::string value4;
  ASSERT_TRUE(db->Get(ReadOptions(), "jkl", &value4).ok() && value4 == "D4");
  std::string value5;
  ASSERT_TRUE(db->Get(ReadOptions(), "mno", &value5).ok() && value5 == "E5");
}

TEST_F(FPTreeDBTest, MergeTest) {
  ASSERT_TRUE(db->Merge(WriteOptions(), "key1", "value1").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &value).ok() && value == "value1");
}

TEST_F(FPTreeDBTest, MultiGetTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "tmpkey", "tmpvalue1").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "tmpkey2", "tmpvalue2").ok());
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

TEST_F(FPTreeDBTest, PutExistingTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &value).ok() && value == "value1");
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value_replaced").ok());
  std::string new_value;
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &new_value).ok() && new_value == "value_replaced");
}

TEST_F(FPTreeDBTest, PutLongStringsTest) {
  std::string big = std::string("ABCDEFGHIJKLMNO QRSTUVWXYZ123:-()4567890ABCDEFGHIJKLMNO QRSTUVW");
  ASSERT_TRUE(db->Put(WriteOptions(), big, big).ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), big, &value).ok() && value == big);
}

TEST_F(FPTreeDBTest, UpdateTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "key2", "value2").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "key3", "value3").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "key2").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "key3", "VALUE3").ok());
  std::string value1;
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &value1).ok() && value1 == "value1");
  std::string value2;
  ASSERT_TRUE(db->Get(ReadOptions(), "key2", &value2).IsNotFound());
  std::string value3;
  ASSERT_TRUE(db->Get(ReadOptions(), "key3", &value3).ok() && value3 == "VALUE3");
}

TEST_F(FPTreeDBTest, WriteTest) {
  WriteBatch batch;
  batch.Delete("key1");
  batch.Put("key2", "value2");
  ASSERT_TRUE(db->Write(WriteOptions(), &batch).IsNotSupported());
}

// =============================================================================================
// REOPEN TESTS
// =============================================================================================

TEST_F(FPTreeDBTest, RODeleteHeadlessTest) {
  Reopen();
  ASSERT_TRUE(db->Delete(WriteOptions(), "nada").ok());
}

TEST_F(FPTreeDBTest, RODeleteNonexistentTest) {
  Reopen();
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "nada").ok());
}

TEST_F(FPTreeDBTest, ROGetHeadlessTest) {
  Reopen();
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "waldo", &value).IsNotFound());
}

TEST_F(FPTreeDBTest, ROGetNonexistentTest) {
  Reopen();
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "waldo", &value).IsNotFound());
}

TEST_F(FPTreeDBTest, ROPutTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  Reopen();
  std::string value1;
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &value1).ok() && value1 == "value1");
}

TEST_F(FPTreeDBTest, ROUpdateTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "key2", "value2").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "key3", "value3").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "key2").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "key3", "VALUE3").ok());
  Reopen();
  std::string value1;
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &value1).ok() && value1 == "value1");
  std::string value2;
  ASSERT_TRUE(db->Get(ReadOptions(), "key2", &value2).IsNotFound());
  std::string value3;
  ASSERT_TRUE(db->Get(ReadOptions(), "key3", &value3).ok() && value3 == "VALUE3");
}
