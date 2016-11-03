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

// Simple example for RocksDB database using NVML backend in place of LSM tree.

#include <iostream>
#include "screedb.h"

#define LOG(msg) std::cout << msg << "\n"

using namespace rocksdb;
using namespace rocksdb::screedb;

const std::string PATH = "/dev/shm/screedb_example";

int main() {
  LOG("Opening database");
  Options options;
  ScreeDB* db;
  Status s = ScreeDB::Open(options, PATH, &db);
  assert(s.ok());

  LOG("Putting new value");
  s = db->Put(WriteOptions(), "key1", "value1");
  assert(s.ok());
  std::string value;
  s = db->Get(ReadOptions(), "key1", &value);
  assert(s.ok() && value == "value1");

  LOG("Replacing existing value");
  std::string value2;
  s = db->Get(ReadOptions(), "key1", &value2);
  assert(s.ok() && value2 == "value1");
  s = db->Put(WriteOptions(), "key1", "value_replaced");
  assert(s.ok());
  std::string value3;
  s = db->Get(ReadOptions(), "key1", &value3);
  assert(s.ok() && value3 == "value_replaced");

  LOG("Deleting existing value");
  s = db->Delete(WriteOptions(), "key1");
  assert(s.ok());
  std::string value4;
  s = db->Get(ReadOptions(), "key1", &value4);
  assert(s.IsNotFound());

  LOG("Closing database");
  delete db;

  LOG("Finished successfully");
  return 0;
}
