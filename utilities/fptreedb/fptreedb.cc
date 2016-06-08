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

#ifndef ROCKSDB_LITE

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <iostream>
#include "rocksdb/utilities/fptreedb.h"

#define NOPE Status::NotSupported()

namespace rocksdb {

  // Open database using specified configuration options and name.
  Status FPTreeDB::Open(const Options& options, const FPTreeDBOptions& dboptions,
                        const std::string& dbname, FPTreeDB** dbptr) {
    std::cout << "[FPTreeDB] Opening database, name=" << dbname << "\n";
    FPTreeDB* impl = new FPTreeDB(options, dboptions, dbname);
    *dbptr = impl;
    std::cout << "[FPTreeDB] Opened ok, name=" << dbname << "\n";
    return Status::OK();
  }

  // Default constructor.
  FPTreeDB::FPTreeDB(const Options& options, const FPTreeDBOptions& dboptions,
                     const std::string& dbname) : dbname_(dbname) {
    std::cout << "[FPTreeDB] Initializing using default constructor, name=" << GetName() << "\n";
    // todo: perform initialization
    std::cout << "[FPTreeDB] Initialized ok, name=" << GetName() << "\n";
  }

  // Safely close the database.
  FPTreeDB::~FPTreeDB() {
    std::cout << "[FPTreeDB] Closing database, name=" << GetName() << "\n";
    // todo: shut down safely
    std::cout << "[FPTreeDB] Closed ok, name=" << GetName() << "\n";
  }

  // Remove the database entry (if any) for "key".  Returns OK on success, and a non-OK status
  // on error.  It is not an error if "key" did not exist in the database.
  Status FPTreeDB::Delete(const WriteOptions& options, ColumnFamilyHandle* column_family,
                          const Slice& key) { return NOPE; }

  // If the database contains an entry for "key" store the corresponding value in *value
  // and return OK. If there is no entry for "key" leave *value unchanged and return a status
  // for which Status::IsNotFound() returns true. May return some other Status on an error.
  Status FPTreeDB::Get(const ReadOptions& options, ColumnFamilyHandle* column_family,
                       const Slice& key, std::string* value) { return NOPE; }

  // If the key definitely does not exist in the database, then this method returns false,
  // else true. If the caller wants to obtain value when the key is found in memory, a bool
  // for 'value_found' must be passed. 'value_found' will be true on return if value has been
  // set properly. This check is potentially lighter-weight than invoking DB::Get(). One way
  // to make this lighter weight is to avoid doing any IOs. Default implementation here returns
  // true and sets 'value_found' to false.
  bool FPTreeDB::KeyMayExist(const ReadOptions& options, ColumnFamilyHandle* column_family,
                             const Slice& key, std::string* value,
                             bool* value_found) { return true; }

  // Merge the database entry for "key" with "value".  Returns OK on success, and a non-OK
  // status on error. The semantics of this operation is determined by the user provided
  // merge_operator when opening DB.
  Status FPTreeDB::Merge(const WriteOptions& options, ColumnFamilyHandle* column_family,
                         const Slice& key, const Slice& value) { return NOPE; }

  // If keys[i] does not exist in the database, then the i'th returned status will be one for
  // which Status::IsNotFound() is true, and (*values)[i] will be set to some arbitrary value
  // (often ""). Otherwise, the i'th returned status will have Status::ok() true, and
  // (*values)[i] will store the value associated with keys[i].
  // (*values) will always be resized to be the same size as (keys).
  // Similarly, the number of returned statuses will be the number of keys.
  // Note: keys will not be "de-duplicated". Duplicate keys will return duplicate values in order.
  std::vector<Status> FPTreeDB::MultiGet(const ReadOptions& options,
                                         const std::vector<ColumnFamilyHandle*>& column_family,
                                         const std::vector<Slice>& keys,
                                         std::vector<std::string>* values) {
    return std::vector<Status>();
  }

  // Set the database entry for "key" to "value". If "key" already exists, it will be overwritten.
  // Returns OK on success, and a non-OK status on error.
  Status FPTreeDB::Put(const WriteOptions& options, ColumnFamilyHandle* column_family,
                       const Slice& key, const Slice& value) { return NOPE; }

  // Apply the specified updates to the database. If `updates` contains no update, WAL will
  // still be synced if options.sync=true. Returns OK on success, non-OK on failure.
  Status FPTreeDB::Write(const WriteOptions& options, WriteBatch* updates) { return NOPE; }

}  // namespace rocksdb

#endif  // ROCKSDB_LITE
