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

// Simple example for RocksDB-style database with "Fingerprinting Persistent Tree" and NVML backend.
// See utilities/fptreedb/fptreedb.cc for implementation.

#include <iostream>
#include "rocksdb/utilities/fptreedb.h"

#define LOG(msg) std::cout << "[fptreedb_example] " << msg << "\n"
#define LOG2(msg) std::cout << "\n[fptreedb_example] " << msg << "\n"

using namespace rocksdb;

std::string kDBPath = "/tmp/fptreedb_example";

int main() {
  LOG("Starting with these data structure sizes:");
  LOG("  sizeof(FPTreeDBRoot) = " << sizeof(FPTreeDBRoot));
  LOG("  sizeof(FPTreeDBKeyValue) = " << sizeof(FPTreeDBKeyValue));
  LOG("  sizeof(FPTreeDBLeaf) = " << sizeof(FPTreeDBLeaf));

  LOG("Setting database options");
  Options options;
  options.create_if_missing = true;  // todo option is ignored, see #7
  FPTreeDBOptions fptree_options;

  LOG("Opening database");
  FPTreeDB* db;
  Status s = FPTreeDB::Open(options, fptree_options, kDBPath, &db);
  assert(s.ok());
  assert(db->GetName() == kDBPath);
  LOG("Database is ready for use");

  LOG2("Delete nonexistent key");
  {
    s = db->Delete(WriteOptions(), "nada");
    assert(s.ok());
  }

  LOG2("Get nonexistent key");
  {
    std::string value;
    s = db->Get(ReadOptions(), "waldo", &value);
    assert(s.IsNotFound());
  }

  LOG2("Put/Get for small value");
  {
    s = db->Put(WriteOptions(), "key1", "value1");
    assert(s.ok());
    std::string value;
    s = db->Get(ReadOptions(), "key1", &value);
    assert(s.ok());
    assert(value == "value1");
  }

  std::string too_big = std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQRSTUVW");

  LOG2("Put/Get for silently truncated key");
  {
    s = db->Put(WriteOptions(), too_big, "value2");
    assert(s.ok());
    std::string value;
    s = db->Get(ReadOptions(), too_big, &value);
    assert(s.IsNotFound()); // because the key was truncated!
    s = db->Get(ReadOptions(), too_big.substr(0, KEY_LENGTH), &value);
    assert(s.ok());
    assert(value == "value2");
  }

  LOG2("Put/Get for silently truncated value");
  {
    s = db->Put(WriteOptions(), "key3", too_big);
    assert(s.ok());
    std::string value;
    s = db->Get(ReadOptions(), "key3", &value);
    assert(s.ok());
    assert(value == too_big.substr(0, VALUE_LENGTH));
  }

  LOG2("Put for existing value");
  {
    std::string value;
    s = db->Get(ReadOptions(), "key1", &value);
    assert(s.ok());
    assert(value == "value1");  // from earlier step
    s = db->Put(WriteOptions(), "key1", "value_replaced");
    assert(s.ok());
    std::string new_value;
    s = db->Get(ReadOptions(), "key1", &new_value);
    assert(s.ok());
    assert(new_value == "value_replaced");
  }

  LOG2("Delete/Get/Delete for existing value");
  {
    s = db->Merge(WriteOptions(), "tmpkey", "tmpvalue1");
    assert(s.ok());
    s = db->Put(WriteOptions(), "tmpkey", "tmpvalue2");
    assert(s.ok());
    s = db->Delete(WriteOptions(), "tmpkey");
    assert(s.ok());
    std::string value;
    s = db->Get(ReadOptions(), "tmpkey", &value);
    assert(s.IsNotFound());
    s = db->Delete(WriteOptions(), "tmpkey");  // no harm in deleting twice
    assert(s.ok());
  }

  LOG2("MultiGet for existing and nonexistent values");
  {
    s = db->Put(WriteOptions(), "tmpkey", "tmpvalue1");
    assert(s.ok());
    s = db->Put(WriteOptions(), "tmpkey2", "tmpvalue2");
    assert(s.ok());
    std::vector<std::string> values = std::vector<std::string>();
    std::vector<Slice> keys = std::vector<Slice>();
    keys.push_back("tmpkey");
    keys.push_back("tmpkey2");
    keys.push_back("tmpkey3");
    keys.push_back("tmpkey");
    std::vector<Status> status = db->MultiGet(ReadOptions(), keys, &values);
    assert(status.size() == 4);
    assert(status.at(0).ok());
    assert(status.at(1).ok());
    assert(status.at(2).IsNotFound());
    assert(status.at(3).ok());
    assert(values.size() == 4);
    assert(values.at(0) == "tmpvalue1");
    assert(values.at(1) == "tmpvalue2");
    assert(values.at(2) == "");
    assert(values.at(3) == "tmpvalue1");
  }

  // Atomic writes not supported yet, see #21
  {
    WriteBatch batch;
    batch.Delete("key1");
    batch.Put("key2", "value2");
    s = db->Write(WriteOptions(), &batch);
    assert(s.IsNotSupported());
    // assert(s.ok());
    // std::string value;
    // s = db->Get(ReadOptions(), "key1", &value);
    // assert(s.IsNotFound());
    // db->Get(ReadOptions(), "key2", &value);
    // assert(value == "value");
  }

  LOG2("Closing database");
  delete db;

  LOG("Finished successfully");
  return 0;
}
