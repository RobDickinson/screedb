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

// Stress test for RocksDB database using NVML backend in place of LSM tree.

#include <iostream>
#include <sys/time.h>
#include "screedb.h"

#define LOG(msg) std::cout << msg << "\n"

using namespace rocksdb;
using namespace rocksdb::screedb;

std::string kDBPath = "/dev/shm/screedb_stress";

unsigned long current_millis() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (unsigned long long) (tv.tv_sec) * 1000 + (unsigned long long) (tv.tv_usec) / 1000;
}

const unsigned long GET_VALUES = 100;
const unsigned long PUT_VALUES = 100000;
const std::string VALUE = "123456789ABCDEFG";

int main() {
  LOG("Opening database");
  Options options;
  options.create_if_missing = true;  // todo option is ignored, see #7
  ScreeDBOptions db_options;
  ScreeDB* db;
  assert(ScreeDB::Open(options, db_options, kDBPath, &db).ok());

  LOG("Putting " << PUT_VALUES << " values");
  unsigned long started = current_millis();
  for (int i = 0; i < PUT_VALUES; i++) {
    assert(db->Put(WriteOptions(), std::to_string(i), VALUE).ok());
  }
  LOG("Put " << PUT_VALUES << " values in " << current_millis() - started << " ms");

  LOG("Getting oldest " << GET_VALUES << " values");
  started = current_millis();
  for (int i = 0; i < GET_VALUES; i++) {
    std::string value;
    assert(db->Get(ReadOptions(), std::to_string(i), &value).ok() && value == VALUE);
  }
  LOG("Got oldest " << GET_VALUES << " values in " << current_millis() - started << " ms");

  LOG("Deleting oldest " << GET_VALUES << " values");
  started = current_millis();
  for (int i = 0; i < GET_VALUES; i++) {
    assert(db->Delete(WriteOptions(), std::to_string(i)).ok());
  }
  LOG("Deleted oldest " << GET_VALUES << " values in " << current_millis() - started << " ms");

  LOG("Closing database");
  delete db;

  LOG("Finished successfully");
  return 0;
}
