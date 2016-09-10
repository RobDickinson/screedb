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

// Implementation for RocksDB database using NVML backend in place of LSM tree.

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <iostream>
#include <unistd.h>
#include "screedb.h"

#define DO_LOG 0
#define LOG(msg) if (DO_LOG) std::cout << "[ScreeDB:" << GetName() << "] " << msg << "\n"

namespace rocksdb {
namespace screedb {

// Open database using specified configuration options and name.
Status ScreeDB::Open(const Options& options, const ScreeDBOptions& dboptions,
                     const std::string& dbname, ScreeDB** dbptr) {
  ScreeDB* impl = new ScreeDB(options, dboptions, dbname);
  *dbptr = impl;
  return Status::OK();
}

// Default constructor.
ScreeDB::ScreeDB(const Options& options, const ScreeDBOptions& dboptions,
                 const std::string& dbname) : dbname_(dbname) {
  LOG("Opening database");
  if (access(GetNamePtr(), F_OK) != 0) {
    LOG("Creating new persistent pool");
    pop_ = pool<ScreeDBRoot>::create(GetNamePtr(), "ScreeDB",          // todo name is hardcoded
                                     PMEMOBJ_MIN_POOL * 64, S_IRWXU);  // todo size is hardcoded
  } else {
    pop_ = pool<ScreeDBRoot>::open(GetNamePtr(), "ScreeDB");           // todo name is hardcoded
  }
  Recover();
  LOG("Opened database ok");
}

// Safely close the database.
ScreeDB::~ScreeDB() {
  LOG("Closing database");
  Shutdown();
  pop_.close();
  LOG("Closed database ok");
}

// ===============================================================================================
// KEY/VALUE METHODS
// ===============================================================================================

// Remove the database entry (if any) for "key".  Returns OK on success, and a non-OK status
// on error.  It is not an error if "key" did not exist in the database.
Status ScreeDB::Delete(const WriteOptions& options, ColumnFamilyHandle* column_family,
                       const Slice& key) {
  LOG("Delete key=" << key.data_);
  auto root = pop_.get_root();
  if (!root->head) {
    LOG("   head not present");
    return Status::OK();
  } else {
    const uint8_t hash = PearsonHash(key.data_);
    auto leaf = root->head;
    while (true) {  // todo unbounded loop
      for (int i = 0; i < LEAF_KEYS; i++) {
        int bmindex = i / 8;
        int bmslot = i % 8;
        if ((leaf->hashes[i] == hash) && ((leaf->bitmaps[bmindex] >> bmslot) & 1)) {
          auto kv = leaf->keyvalues[i];
          if (strcmp(kv->key_ptr.get(), key.data_) == 0) {
            LOG("   updating slot, bmindex=" << bmindex << ", bmslot=" << bmslot);
            transaction::exec_tx(pop_, [&] {
              kv->key_ptr[0] = 0;  // todo not reclaiming allocated key/value storage
            });
          }
        }
      }
      if (leaf->next) {
        leaf = leaf->next;
      } else {
        LOG("   could not find key");
        break;
      }
    }
  }
  return Status::OK();
}

// If the database contains an entry for "key" store the corresponding value in *value
// and return OK. If there is no entry for "key" leave *value unchanged and return a status
// for which Status::IsNotFound() returns true. May return some other Status on an error.
Status ScreeDB::Get(const ReadOptions& options, ColumnFamilyHandle* column_family,
                    const Slice& key, std::string* value) {
  LOG("Get key=" << key.data_);
  auto root = pop_.get_root();
  if (!root->head) {
    LOG("   head not present");
    return Status::NotFound();
  } else {
    const uint8_t hash = PearsonHash(key.data_);
    auto leaf = root->head;
    while (true) {  // todo unbounded loop
      for (int i = (LEAF_KEYS - 1); i >= 0; i--) {  // reverse order to read most recent writes
        int bmindex = i / 8;
        int bmslot = i % 8;
        if ((leaf->hashes[i] == hash) && ((leaf->bitmaps[bmindex] >> bmslot) & 1)) {
          auto kv = leaf->keyvalues[i];
          if (strcmp(kv->key_ptr.get(), key.data_) == 0) {
            value->append(kv->value_ptr.get());
            LOG("   found value=" << kv->value_ptr.get() << ", bmindex=" << bmindex
                                  << ", bmslot=" << bmslot);
            return Status::OK();
          }
        }
      }
      if (leaf->next) {
        leaf = leaf->next;
      } else {
        LOG("   could not find key");
        return Status::NotFound();
      }
    }
  }
}

// If keys[i] does not exist in the database, then the i'th returned status will be one for
// which Status::IsNotFound() is true, and (*values)[i] will be set to some arbitrary value
// (often ""). Otherwise, the i'th returned status will have Status::ok() true, and
// (*values)[i] will store the value associated with keys[i].
// (*values) will always be resized to be the same size as (keys).
// Similarly, the number of returned statuses will be the number of keys.
// Note: keys will not be "de-duplicated". Duplicate keys will return duplicate values in order.
std::vector<Status> ScreeDB::MultiGet(const ReadOptions& options,
                                      const std::vector<ColumnFamilyHandle*>& column_family,
                                      const std::vector<Slice>& keys,
                                      std::vector<std::string>* values) {
  LOG("MultiGet for " << keys.size() << " keys");
  std::vector<Status> status = std::vector<Status>();
  for (auto& key: keys) {
    std::string value;
    Status s = Get(options, key.data_, &value);
    status.push_back(s);
    values->push_back(s.ok() ? value : "");
  }
  LOG("MultiGet done for " << keys.size() << " keys");
  return status;
}

// Set the database entry for "key" to "value". If "key" already exists, it will be overwritten.
// Returns OK on success, and a non-OK status on error.
Status ScreeDB::Put(const WriteOptions& options, ColumnFamilyHandle* column_family,
                    const Slice& key, const Slice& value) {
  LOG("Put key=" << key.data_ << ", value=" << value.data_);
  auto root = pop_.get_root();
  auto leaf = root->head;

  // attempt to add to head leaf first
  if (leaf) {
    for (int i = 0; i < LEAF_KEYS; i++) {
      int bmindex = i / 8;
      int bmslot = i % 8;
      if ((leaf->bitmaps[bmindex] >> bmslot) & 1) continue;
      LOG("   updating head slot, bmindex=" << bmindex << ", bmslot=" << bmslot);
      transaction::exec_tx(pop_, [&] {
        LeafFillSlot(leaf, i, bmindex, bmslot, key, value);
      });
      return Status::OK();
    }
  }

  // add new leaf in head position
  LOG("   adding new leaf");
  transaction::exec_tx(pop_, [&] {
    root->head = make_persistent<ScreeDBLeaf>();
    root->head->next = leaf;
    LeafFillSlot(root->head, 0, 0, 0, key, value);
  });
  return Status::OK();
}

// ===============================================================================================
// PROTECTED LEAF METHODS
// ===============================================================================================

void ScreeDB::LeafDelete() {

}

void ScreeDB::LeafFillSlot(const LEAF_PTR_T leaf, const int slot, const int bmindex,
                           const int bmslot, const Slice& key, const Slice& value) {
  leaf->bitmaps[bmindex] = leaf->bitmaps[bmindex] ^ (1 << bmslot);     // toggle bitmap slot bit
  leaf->hashes[slot] = PearsonHash(key.data_);                         // calculate & store hash
  leaf->keyvalues[slot] = make_persistent<ScreeDBKeyValue>();          // make key/value object
  auto kv = leaf->keyvalues[slot];
  kv->key_ptr = make_persistent<char[]>(key.size() + 1);               // reserve space for key
  kv->value_ptr = make_persistent<char[]>(value.size() + 1);           // reserve space for value
  memcpy(kv->key_ptr.get(), key.data_, key.size());                    // copy key data
  memcpy(kv->value_ptr.get(), value.data_, value.size());              // copy value data
}

void ScreeDB::LeafFind(const Slice& key) {

}

void ScreeDB::LeafSplit() {

}

// ===============================================================================================
// PROTECTED LIFECYCLE METHODS
// ===============================================================================================

void ScreeDB::Recover() {
  LOG("Recovering database");

  // Create root if not already present
  // @todo handle opened/closed inequality, including count correction
  auto root = pop_.get_root();
  if (!root->head) {
    LOG("   creating root");
    transaction::exec_tx(pop_, [&] {
      root->opened = 1;
      root->closed = 0;
    });
  } else {
    LOG("   recovering head: opened=" << root->opened << ", closed=" << root->closed);
    transaction::exec_tx(pop_, [&] { root->opened = root->opened + 1; });
  }

  RebuildInnerNodes();
  LOG("Recovered database ok");
}

void ScreeDB::RebuildInnerNodes() {
  LOG("Rebuilding inner nodes");
  auto root = pop_.get_root();
  if (root->head) {
    auto leaf = root->head;
    for (int i = 0; true; i++) {
      auto kv = leaf->keyvalues[0];
      LOG("   leaf[" << i << "] key=" << kv->key_ptr.get() << ", value=" << kv->value_ptr.get());
      if (leaf->next) leaf = leaf->next; else break;
    }
  }
  LOG("Rebuilt inner nodes ok");
}

void ScreeDB::Shutdown() {
  LOG("Shutting down database");
  auto root = pop_.get_root();
  transaction::exec_tx(pop_, [&] { root->closed = root->closed + 1; });
  LOG("Shut down database ok");
}

// ===============================================================================================
// PROTECTED HELPER METHODS
// ===============================================================================================

// Pearson hashing lookup table from RFC 3074
const uint8_t PEARSON_LOOKUP_TABLE[256] = {
        251, 175, 119, 215, 81, 14, 79, 191, 103, 49, 181, 143, 186, 157, 0,
        232, 31, 32, 55, 60, 152, 58, 17, 237, 174, 70, 160, 144, 220, 90, 57,
        223, 59, 3, 18, 140, 111, 166, 203, 196, 134, 243, 124, 95, 222, 179,
        197, 65, 180, 48, 36, 15, 107, 46, 233, 130, 165, 30, 123, 161, 209, 23,
        97, 16, 40, 91, 219, 61, 100, 10, 210, 109, 250, 127, 22, 138, 29, 108,
        244, 67, 207, 9, 178, 204, 74, 98, 126, 249, 167, 116, 34, 77, 193,
        200, 121, 5, 20, 113, 71, 35, 128, 13, 182, 94, 25, 226, 227, 199, 75,
        27, 41, 245, 230, 224, 43, 225, 177, 26, 155, 150, 212, 142, 218, 115,
        241, 73, 88, 105, 39, 114, 62, 255, 192, 201, 145, 214, 168, 158, 221,
        148, 154, 122, 12, 84, 82, 163, 44, 139, 228, 236, 205, 242, 217, 11,
        187, 146, 159, 64, 86, 239, 195, 42, 106, 198, 118, 112, 184, 172, 87,
        2, 173, 117, 176, 229, 247, 253, 137, 185, 99, 164, 102, 147, 45, 66,
        231, 52, 141, 211, 194, 206, 246, 238, 56, 110, 78, 248, 63, 240, 189,
        93, 92, 51, 53, 183, 19, 171, 72, 50, 33, 104, 101, 69, 8, 252, 83, 120,
        76, 135, 85, 54, 202, 125, 188, 213, 96, 235, 136, 208, 162, 129, 190,
        132, 156, 38, 47, 1, 7, 254, 24, 4, 216, 131, 89, 21, 28, 133, 37, 153,
        149, 80, 170, 68, 6, 169, 234, 151
};

// Pearson hashing algorithm from RFC 3074
uint8_t ScreeDB::PearsonHash(const char* data) {
  size_t len = strlen(data);
  uint8_t hash = (uint8_t) len;
  for (size_t i = len; i > 0;) {
    hash = PEARSON_LOOKUP_TABLE[hash ^ data[--i]];
  }
  return hash;
}

} // namespace screedb
} // namespace rocksdb
