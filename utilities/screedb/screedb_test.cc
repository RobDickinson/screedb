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

// Unit tests for RocksDB database using NVML backend in place of LSM.

#include "screedb.h"
#include "gtest/gtest.h"

using namespace rocksdb;
using namespace rocksdb::screedb;

std::string kDBPath = "/dev/shm/screedb_test";

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class ScreeDBTest : public testing::Test {
public:
  ScreeDB* db;

  ScreeDBTest() {
    std::remove(kDBPath.c_str());
    Open();
  }

  ~ScreeDBTest() { delete db; }

  void Reopen() {
    delete db;
    Open();
  }

private:
  void Open() {
    Options options;
    options.create_if_missing = true;  // todo options are ignored, see #7
    ScreeDBOptions db_options;
    Status s = ScreeDB::Open(options, db_options, kDBPath, &db);
    assert(s.ok() && db->GetName() == kDBPath);
  }
};

// =============================================================================================
// TEST SINGLE-LEAF TREE
// =============================================================================================

TEST_F(ScreeDBTest, SizeofTest) {
  // persistent types
  ASSERT_TRUE(sizeof(ScreeDBRoot) == 32);
  ASSERT_TRUE(sizeof(ScreeDBLeaf) == 3136);
  ASSERT_TRUE(sizeof_field(ScreeDBLeaf, hashes) + sizeof_field(ScreeDBLeaf, next) == 64);
  ASSERT_TRUE(sizeof(ScreeDBString) == 32);

  // volatile types
  ASSERT_TRUE(sizeof(ScreeDBInnerNode) == 232);
  ASSERT_TRUE(sizeof(ScreeDBLeafNode) == 40);
}

TEST_F(ScreeDBTest, DeleteAllTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "tmpkey", "tmpvalue1").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "tmpkey").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "tmpkey1", "tmpvalue1").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "tmpkey1", &value).ok() && value == "tmpvalue1");
}

TEST_F(ScreeDBTest, DeleteExistingTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "tmpkey1", "tmpvalue1").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "tmpkey2", "tmpvalue2").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "tmpkey1").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "tmpkey1").ok()); // ok to delete twice
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "tmpkey1", &value).IsNotFound());
  ASSERT_TRUE(db->Get(ReadOptions(), "tmpkey2", &value).ok() && value == "tmpvalue2");
}

TEST_F(ScreeDBTest, DeleteHeadlessTest) {
  ASSERT_TRUE(db->Delete(WriteOptions(), "nada").ok());
}

TEST_F(ScreeDBTest, DeleteNonexistentTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "nada").ok());
}

TEST_F(ScreeDBTest, EmptyKeyTest) {                                      // todo correct behavior?
  ASSERT_TRUE(db->Put(WriteOptions(), "", "blah").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "", &value).ok() && value == "blah");
}

TEST_F(ScreeDBTest, EmptyValueTest) {                                    // todo correct behavior?
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &value).ok() && value == "");
}

TEST_F(ScreeDBTest, GetAppendToExternalValueTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "cool").ok());
  std::string value = "super";
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &value).ok() && value == "supercool");
}

TEST_F(ScreeDBTest, GetHeadlessTest) {
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "waldo", &value).IsNotFound());
}

TEST_F(ScreeDBTest, GetMultipleTest) {
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

TEST_F(ScreeDBTest, GetMultipleAfterDeleteTest) {
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

TEST_F(ScreeDBTest, GetNonexistentTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "waldo", &value).IsNotFound());
}

TEST_F(ScreeDBTest, MergeTest) {
  ASSERT_TRUE(db->Merge(WriteOptions(), "key1", "value1").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &value).ok() && value == "value1");
}

TEST_F(ScreeDBTest, MultiGetTest) {
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

TEST_F(ScreeDBTest, PutExistingTest) {
  std::string value;
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &value).ok() && value == "value1");

  std::string new_value;
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "VALUE1").ok());           // same length
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &new_value).ok() && new_value == "VALUE1");

  std::string new_value2;
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "new_value").ok());        // longer length
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &new_value2).ok() && new_value2 == "new_value");

  std::string new_value3;
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "?").ok());                // shorter length
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &new_value3).ok() && new_value3 == "?");
}

TEST_F(ScreeDBTest, PutKeysOfDifferentLengthsTest) {
  std::string value;
  ASSERT_TRUE(db->Put(WriteOptions(), "123456789ABCDE", "A").ok());      // 2 under the sso limit
  ASSERT_TRUE(db->Get(ReadOptions(), "123456789ABCDE", &value).ok() && value == "A");

  std::string value2;
  ASSERT_TRUE(db->Put(WriteOptions(), "123456789ABCDEF", "B").ok());     // 1 under the sso limit
  ASSERT_TRUE(db->Get(ReadOptions(), "123456789ABCDEF", &value2).ok() && value2 == "B");

  std::string value3;
  ASSERT_TRUE(db->Put(WriteOptions(), "123456789ABCDEFG", "C").ok());    // at the sso limit
  ASSERT_TRUE(db->Get(ReadOptions(), "123456789ABCDEFG", &value3).ok() && value3 == "C");

  std::string value4;
  ASSERT_TRUE(db->Put(WriteOptions(), "123456789ABCDEFGH", "D").ok());   // 1 over the sso limit
  ASSERT_TRUE(db->Get(ReadOptions(), "123456789ABCDEFGH", &value4).ok() && value4 == "D");

  std::string value5;
  ASSERT_TRUE(db->Put(WriteOptions(), "123456789ABCDEFGHI", "E").ok());   // 2 over the sso limit
  ASSERT_TRUE(db->Get(ReadOptions(), "123456789ABCDEFGHI", &value5).ok() && value5 == "E");
}

TEST_F(ScreeDBTest, PutValuesOfDifferentLengthsTest) {
  std::string value;
  ASSERT_TRUE(db->Put(WriteOptions(), "A", "123456789ABCDE").ok());      // 2 under the sso limit
  ASSERT_TRUE(db->Get(ReadOptions(), "A", &value).ok() && value == "123456789ABCDE");

  std::string value2;
  ASSERT_TRUE(db->Put(WriteOptions(), "B", "123456789ABCDEF").ok());     // 1 under the sso limit
  ASSERT_TRUE(db->Get(ReadOptions(), "B", &value2).ok() && value2 == "123456789ABCDEF");

  std::string value3;
  ASSERT_TRUE(db->Put(WriteOptions(), "C", "123456789ABCDEFG").ok());    // at the sso limit
  ASSERT_TRUE(db->Get(ReadOptions(), "C", &value3).ok() && value3 == "123456789ABCDEFG");

  std::string value4;
  ASSERT_TRUE(db->Put(WriteOptions(), "D", "123456789ABCDEFGH").ok());   // 1 over the sso limit
  ASSERT_TRUE(db->Get(ReadOptions(), "D", &value4).ok() && value4 == "123456789ABCDEFGH");

  std::string value5;
  ASSERT_TRUE(db->Put(WriteOptions(), "E", "123456789ABCDEFGHI").ok());  // 2 over the sso limit
  ASSERT_TRUE(db->Get(ReadOptions(), "E", &value5).ok() && value5 == "123456789ABCDEFGHI");
}

TEST_F(ScreeDBTest, WriteTest) {
  WriteBatch batch;
  batch.Delete("key1");
  batch.Put("key2", "value2");
  ASSERT_TRUE(db->Write(WriteOptions(), &batch).IsNotSupported());
}

// =============================================================================================
// TEST RECOVERY OF SINGLE-LEAF TREE
// =============================================================================================

TEST_F(ScreeDBTest, DeleteHeadlessAfterRecoveryTest) {
  Reopen();
  ASSERT_TRUE(db->Delete(WriteOptions(), "nada").ok());
}

TEST_F(ScreeDBTest, DeleteNonexistentAfterRecoveryTest) {
  Reopen();
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  ASSERT_TRUE(db->Delete(WriteOptions(), "nada").ok());
}

TEST_F(ScreeDBTest, GetHeadlessAfterRecoveryTest) {
  Reopen();
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "waldo", &value).IsNotFound());
}

TEST_F(ScreeDBTest, GetMultipleAfterRecoveryTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "abc", "A1").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "def", "B2").ok());
  ASSERT_TRUE(db->Put(WriteOptions(), "hij", "C3").ok());
  Reopen();
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

TEST_F(ScreeDBTest, GetNonexistentAfterRecoveryTest) {
  Reopen();
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  std::string value;
  ASSERT_TRUE(db->Get(ReadOptions(), "waldo", &value).IsNotFound());
}

TEST_F(ScreeDBTest, PutAfterRecoveryTest) {
  ASSERT_TRUE(db->Put(WriteOptions(), "key1", "value1").ok());
  Reopen();
  std::string value1;
  ASSERT_TRUE(db->Get(ReadOptions(), "key1", &value1).ok() && value1 == "value1");
}

TEST_F(ScreeDBTest, UpdateAfterRecoveryTest) {
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

// =============================================================================================
// TEST MULTIPLE-LEAF TREE (ONE INNER NODE ONLY)
// =============================================================================================

TEST_F(ScreeDBTest, MultipleLeafNodeAscendingTest) {
  for (int i = 10000; i <= (10000 + NODE_KEYS * 8); i++) {
    std::string istr = std::to_string(i);
    assert(db->Put(WriteOptions(), istr, istr).ok());
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == istr);
  }
  for (int i = 10000; i <= (10000 + NODE_KEYS * 8); i++) {
    std::string istr = std::to_string(i);
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == istr);
  }
}

TEST_F(ScreeDBTest, MultipleLeafNodeAscendingTest2) {
  for (int i = 1; i <= NODE_KEYS * 8; i++) {
    std::string istr = std::to_string(i);
    assert(db->Put(WriteOptions(), istr, istr).ok());
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == istr);
  }
  for (int i = 1; i <= NODE_KEYS * 8; i++) {
    std::string istr = std::to_string(i);
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == istr);
  }
}

TEST_F(ScreeDBTest, MultipleLeafNodeDescendingTest) {
  for (int i = (10000 + NODE_KEYS * 8); i >= 10000; i--) {
    std::string istr = std::to_string(i);
    assert(db->Put(WriteOptions(), istr, istr).ok());
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == istr);
  }
  for (int i = (10000 + NODE_KEYS * 8); i >= 10000; i--) {
    std::string istr = std::to_string(i);
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == istr);
  }
}

TEST_F(ScreeDBTest, MultipleLeafNodeDescendingTest2) {
  for (int i = NODE_KEYS * 8; i >= 1; i--) {
    std::string istr = std::to_string(i);
    assert(db->Put(WriteOptions(), istr, istr).ok());
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == istr);
  }
  for (int i = NODE_KEYS * 8; i >= 1; i--) {
    std::string istr = std::to_string(i);
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == istr);
  }
}

// todo need delete tests for multiple leaves

// =============================================================================================
// TEST RECOVERY OF MULTIPLE-LEAF TREE (ONE INNER NODE ONLY)
// =============================================================================================

// todo not yet recovering inner nodes

// =============================================================================================
// TEST NESTED-INNER TREE
// =============================================================================================

TEST_F(ScreeDBTest, NestedInnerNodeAscendingTest) {
  for (int i = 1; i <= 999999; i++) {
    std::string istr = std::to_string(i);
    assert(db->Put(WriteOptions(), istr, (istr + "!")).ok());
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == (istr + "!"));
  }
  for (int i = 1; i <= 999999; i++) {
    std::string istr = std::to_string(i);
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == (istr + "!"));
  }
}

TEST_F(ScreeDBTest, NestedInnerNodeDescendingTest) {
  for (int i = 999999; i >= 1; i--) {
    std::string istr = std::to_string(i);
    assert(db->Put(WriteOptions(), istr, ("ABC" + istr)).ok());
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == ("ABC" + istr));
  }
  for (int i = 999999; i >= 1; i--) {
    std::string istr = std::to_string(i);
    std::string value;
    assert(db->Get(ReadOptions(), istr, &value).ok() && value == ("ABC" + istr));
  }
}

// todo need delete tests for nested inner nodes

// =============================================================================================
// TEST RECOVERY OF NESTED-INNER TREE
// =============================================================================================

// todo not yet recovering inner nodes
