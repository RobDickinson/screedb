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
    auto leaf = root->head;
    while (true) {  // todo unbounded loop
      for (int i = 0; i < LEAF_KEYS; i++) {
        int bmindex = i / 8;
        int bmslot = i % 8;
        if ((leaf->bitmaps[bmindex] >> bmslot) & 1) {
          // todo check against hashes before reading the keyvalues array
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
    auto leaf = root->head;
    while (true) {  // todo unbounded loop
      for (int i = (LEAF_KEYS - 1); i >= 0; i--) {  // reverse order to read most recent writes
        int bmindex = i / 8;
        int bmslot = i % 8;
        if ((leaf->bitmaps[bmindex] >> bmslot) & 1) {
          // todo check against hashes before reading the keyvalues array
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

  // attempt to add to head leaf first
  if (root->head) {
    auto leaf = root->head;
    for (int i = 0; i < LEAF_KEYS; i++) {
      int bmindex = i / 8;
      int bmslot = i % 8;
      if ((leaf->bitmaps[bmindex] >> bmslot) & 1) continue;
      LOG("   updating head slot, bmindex=" << bmindex << ", bmslot=" << bmslot);
      transaction::exec_tx(pop_, [&] {
        leaf->bitmaps[bmindex] = leaf->bitmaps[bmindex] ^ (1 << bmslot);
        leaf->keyvalues[i] = make_persistent<ScreeDBKeyValue>();
        KeyValueFill(leaf->keyvalues[i], key, value);
      });
      return Status::OK();
    }
  }

  // add new leaf in head position
  LOG("   adding new leaf");
  auto old_head = root->head;
  transaction::exec_tx(pop_, [&] {
    root->head = make_persistent<ScreeDBLeaf>();
    auto head = root->head;
    head->next = old_head;
    head->bitmaps[0] = 1;
    head->keyvalues[0] = make_persistent<ScreeDBKeyValue>();
    KeyValueFill(head->keyvalues[0], key, value);
  });
  return Status::OK();
}

// ===============================================================================================
// PROTECTED LEAF METHODS
// ===============================================================================================

void ScreeDB::KeyValueFill(const LEAF_KEYVALUE_T kv, const Slice& key, const Slice& value) {
  kv->key_ptr = make_persistent<char[]>(key.size() + 1);
  kv->value_ptr = make_persistent<char[]>(value.size() + 1);
  memcpy(kv->key_ptr.get(), key.data_, key.size());
  memcpy(kv->value_ptr.get(), value.data_, value.size());
}

void ScreeDB::LeafDelete() {

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

} // namespace screedb
} // namespace rocksdb
