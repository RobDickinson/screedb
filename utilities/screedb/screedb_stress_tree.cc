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

// Stress test for persistent tree using NVML backend.

#include <iostream>
#include <sys/time.h>
#include "screedb.h"

#define LOG(msg) std::cout << msg << "\n"

using namespace rocksdb::screedb;

const unsigned long COUNT = 30000000;

unsigned long current_millis() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (unsigned long long) (tv.tv_sec) * 1000 + (unsigned long long) (tv.tv_usec) / 1000;
}

void testDelete(ScreeDBTree* impl) {
  auto started = current_millis();
  for (int i = 0; i < COUNT; i++) { impl->Delete(std::to_string(i)); }
  LOG("   in " << current_millis() - started << " ms");
}

void testGet(ScreeDBTree* impl) {
  auto started = current_millis();
  std::string value;
  for (int i = 0; i < COUNT; i++) { impl->Get(std::to_string(i), &value); }
  LOG("   in " << current_millis() - started << " ms");
}

void testPut(ScreeDBTree* impl) {
  auto started = current_millis();
  for (int i = 0; i < COUNT; i++) {
    std::string str = std::to_string(i);
    impl->Put(str, str);
  }
  LOG("   in " << current_millis() - started << " ms");
}

int main() {
  LOG("Opening");
  ScreeDBTree* impl = new ScreeDBTree("/dev/shm/screedb");

  LOG("Inserting " << COUNT << " values");
  testPut(impl);
  LOG("Getting " << COUNT << " values");
  testGet(impl);
  LOG("Updating " << COUNT << " values");
  testPut(impl);
  LOG("Deleting " << COUNT << " values");
  testDelete(impl);
  LOG("Reinserting " << COUNT << " values");
  testPut(impl);

  LOG("Closing");
  delete impl;
  LOG("Finished");
  return 0;
}
