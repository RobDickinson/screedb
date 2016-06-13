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

#include <iostream>
#include "rocksdb/utilities/fptreedb.h"

using namespace rocksdb;

std::string kDBPath = "/tmp/fptreedb_example";

int main() {
  std::cout << "[fptreedb_example] Starting\n";

  FPTreeDB* db;
  Options options;
  options.create_if_missing = true;
  FPTreeDBOptions fptree_options;

  // open DB
  Status s = FPTreeDB::Open(options, fptree_options, kDBPath, &db);
  assert(s.ok());
  assert(db->GetName() == kDBPath);

  // put key-value
  s = db->Put(WriteOptions(), "key1", "value");
  assert(s.IsNotSupported());

//  TEMPORARILY DISABLED:
//  assert(s.ok());
//  std::string value;
//  // get value
//  s = db->Get(ReadOptions(), "key1", &value);
//  assert(s.ok());
//  assert(value == "value");
//
//  // atomically apply a set of updates
//  {
//    WriteBatch batch;
//    batch.Delete("key1");
//    batch.Put("key2", value);
//    s = db->Write(WriteOptions(), &batch);
//  }
//
//  s = db->Get(ReadOptions(), "key1", &value);
//  assert(s.IsNotFound());
//
//  db->Get(ReadOptions(), "key2", &value);
//  assert(value == "value");

  // safely close DB
  delete db;

  std::cout << "[fptreedb_example] Finished successfully\n";
  return 0;
}
