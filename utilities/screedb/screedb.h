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

// Header for RocksDB database using NVML backend in place of LSM tree.

#pragma once

#include <string>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>
#include "rocksdb/db.h"

#define NOOPE override { return Status::NotSupported(); }
#define sizeof_field(type, field) sizeof(((type *)0)->field)

using nvml::obj::p;
using nvml::obj::persistent_ptr;
using nvml::obj::make_persistent;
using nvml::obj::transaction;
using nvml::obj::delete_persistent;
using nvml::obj::pool;

namespace rocksdb {
namespace screedb {

#define INNER_KEYS 4                                       // maximum keys for inner nodes
#define INNER_KEYS_MIDPOINT (INNER_KEYS / 2)               // halfway point within the node
#define INNER_KEYS_UPPER ((INNER_KEYS / 2) + 1)            // index where upper half of keys begins
#define NODE_KEYS 48                                       // maximum keys in tree nodes
#define NODE_KEYS_MIDPOINT 24                              // halfway point within the node
#define SSO_CHARS 15                                       // chars for short string optimization
#define SSO_SIZE (SSO_CHARS + 1)                           // sso chars plus null terminator

class ScreeDBString {                                      // persistent string class
public:                                                    // start public fields and methods
  char* data() const;                                      // returns data as c-style string
  void set(const Slice& slice);                            // copy data from c-style string
private:                                                   // start private fields and methods
  char sso[SSO_SIZE];                                      // local storage for short strings
  persistent_ptr<char[]> str;                              // pointer to storage for longer strings
};

struct ScreeDBLeaf {                                       // persistent leaves of the tree
  p<uint8_t> hashes[NODE_KEYS];                            // 48 bytes, Pearson hashes of keys
  persistent_ptr<ScreeDBLeaf> next;                        // 16 bytes, points to next leaf
  p<ScreeDBString> kv_keys[NODE_KEYS];                     // key strings stored in this leaf
  p<ScreeDBString> kv_values[NODE_KEYS];                   // value strings stored in this leaf
};

struct ScreeDBRoot {                                       // persistent root object
  p<uint64_t> opened;                                      // number of times opened
  p<uint64_t> closed;                                      // number of times closed safely
  persistent_ptr<ScreeDBLeaf> head;                        // head of linked list of leaves
};

struct ScreeDBNode {                                       // volatile nodes of the tree
  bool is_leaf = false;                                    // indicate inner or leaf node
  ScreeDBNode* parent;                                     // parent of this node (null if top)
};

struct ScreeDBInnerNode : ScreeDBNode {                    // volatile inner nodes of the tree
  uint8_t keycount;                                        // count of keys in this node
  std::string keys[INNER_KEYS + 1];                        // child keys plus one overflow slot
  ScreeDBNode* children[INNER_KEYS + 2];                   // child nodes plus one overflow slot
};

struct ScreeDBLeafNode : ScreeDBNode {                     // volatile leaf nodes of the tree
  uint8_t hashes[NODE_KEYS];                               // Pearson hashes of keys
  persistent_ptr<ScreeDBLeaf> leaf;                        // pointer to persistent leaf
  bool lock;                                               // boolean modification lock
};

class ScreeDBTree {                                        // persistent tree implementation
public:
  ScreeDBTree(const std::string& name);
  ~ScreeDBTree();
  const std::string& GetName() const { return name; }
  const char* GetNamePtr() const { return name.c_str(); }
  Status Delete(const Slice& key);
  Status Get(const Slice& key, std::string* value);
  std::vector<Status> MultiGet(const std::vector<Slice>& keys,
                               std::vector<std::string>* values);
  Status Put(const Slice& key, const Slice& value);
protected:
  void LeafDebugDump(ScreeDBNode* node);
  void LeafDebugDumpWithChildren(ScreeDBInnerNode* inner);
  void LeafFillFirstEmptySlot(ScreeDBLeafNode* leafnode, const uint8_t hash,
                              const Slice& key, const Slice& value);
  bool LeafFillSlotForKey(ScreeDBLeafNode* leafnode, const uint8_t hash,
                          const Slice& key, const Slice& value);
  void LeafFillSpecificSlot(ScreeDBLeafNode* leafnode, const uint8_t hash,
                            const Slice& key, const Slice& value, const int slot);
  ScreeDBLeafNode* LeafSearch(const Slice& key);
  void LeafSplit(ScreeDBLeafNode* leafnode, const uint8_t hash,
                 const Slice& key, const Slice& value);
  void LeafUpdateParentsAfterSplit(ScreeDBNode* node, ScreeDBNode* new_node,
                                   std::string* split_key);
  uint8_t PearsonHash(const char* data, const size_t size);
  void RebuildNodes();
  void Recover();
  void Shutdown();
private:
  ScreeDBTree(const ScreeDBTree&);                         // prevent copying
  void operator=(const ScreeDBTree&);                      // prevent assignment
  const std::string name;                                  // name when constructed
  pool<ScreeDBRoot> pop_;                                  // pool for persistent root
  ScreeDBNode* top_ = nullptr;                             // top of volatile tree
};

class ScreeDB : public DB {                                // RocksDB API on persistent tree
public:
  // Open database using specified configuration options and name.
  static Status Open(const Options& options, const std::string& dbname, ScreeDB** dbptr);

  // Safely close the database.
  virtual ~ScreeDB();

  // =============================================================================================
  // KEY/VALUE METHODS
  // =============================================================================================

  // Remove the database entry (if any) for "key".  Returns OK on success, and a non-OK status
  // on error.  It is not an error if "key" did not exist in the database.
  using DB::Delete;
  virtual Status Delete(const WriteOptions& options, ColumnFamilyHandle* column_family,
                        const Slice& key) override {
    return dbtree->Delete(key);
  }

  // If the database contains an entry for "key" store the corresponding value in *value
  // and return OK. If there is no entry for "key" leave *value unchanged and return a status
  // for which Status::IsNotFound() returns true. May return some other Status on an error.
  using DB::Get;
  virtual Status Get(const ReadOptions& options, ColumnFamilyHandle* column_family,
                     const Slice& key, std::string* value) override {
    return dbtree->Get(key, value);
  }

  // If the key definitely does not exist in the database, then this method returns false,
  // else true. If the caller wants to obtain value when the key is found in memory, a bool
  // for 'value_found' must be passed. 'value_found' will be true on return if value has been
  // set properly. This check is potentially lighter-weight than invoking DB::Get(). One way
  // to make this lighter weight is to avoid doing any IOs. Default implementation here returns
  // true and sets 'value_found' to false.
  using DB::KeyMayExist;
  virtual bool KeyMayExist(const ReadOptions& options, ColumnFamilyHandle* column_family,
                           const Slice& key, std::string* value,
                           bool* value_found = nullptr) override { return true; }

  // Merge the database entry for "key" with "value".  Returns OK on success, and a non-OK
  // status on error. The semantics of this operation is determined by the user provided
  // merge_operator when opening DB.
  using DB::Merge;
  virtual Status Merge(const WriteOptions& options, ColumnFamilyHandle* column_family,
                       const Slice& key, const Slice& value) {
    return Put(options, column_family, key, value);  // todo not using merge_operator, see #7
  }

  // If keys[i] does not exist in the database, then the i'th returned status will be one for
  // which Status::IsNotFound() is true, and (*values)[i] will be set to some arbitrary value
  // (often ""). Otherwise, the i'th returned status will have Status::ok() true, and
  // (*values)[i] will store the value associated with keys[i].
  // (*values) will always be resized to be the same size as (keys).
  // Similarly, the number of returned statuses will be the number of keys.
  // Note: keys will not be "de-duplicated". Duplicate keys will return duplicate values in order.
  using DB::MultiGet;
  virtual std::vector<Status> MultiGet(const ReadOptions& options,
                                       const std::vector<ColumnFamilyHandle*>& column_family,
                                       const std::vector<Slice>& keys,
                                       std::vector<std::string>* values) override {
    return dbtree->MultiGet(keys, values);
  }

  // Set the database entry for "key" to "value". If "key" already exists, it will be overwritten.
  // Returns OK on success, and a non-OK status on error.
  using DB::Put;
  virtual Status Put(const WriteOptions& options, ColumnFamilyHandle* column_family,
                     const Slice& key, const Slice& value) override {
    return dbtree->Put(key, value);
  }

  // Remove the database entry for "key". Requires that the key exists and was not overwritten.
  // Returns OK on success, and a non-OK status on error.  It is not an error if "key" did not
  // exist in the database.
  //
  // If a key is overwritten (by calling Put() multiple times), then the result of calling
  // SingleDelete() on this key is undefined.  SingleDelete() only behaves correctly if there
  // has been only one Put() for this key since the previous call to SingleDelete() for this key.
  //
  // This feature is currently an experimental performance optimization for a very specific
  // workload.  It is up to the caller to ensure that SingleDelete is only used for a key that
  // is not deleted using Delete() or written using Merge().  Mixing SingleDelete operations
  // with Deletes and Merges can result in undefined behavior.
  using DB::SingleDelete;
  virtual Status SingleDelete(const WriteOptions& options, ColumnFamilyHandle* column_family,
                              const Slice& key) NOOPE;

  // Apply the specified updates to the database. If `updates` contains no update, WAL will
  // still be synced if options.sync=true. Returns OK on success, non-OK on failure.
  using DB::Write;
  virtual Status Write(const WriteOptions& options, WriteBatch* updates) NOOPE;

  // =============================================================================================
  // ITERATOR METHODS
  // =============================================================================================

  // Return a heap-allocated iterator over the contents of the database. The result of is
  // initially invalid (caller must call one of the Seek methods on the iterator before using it).
  // Caller should delete the iterator when it is no longer needed. The returned iterator
  // should be deleted before this db is deleted.
  using DB::NewIterator;
  virtual Iterator* NewIterator(const ReadOptions& options,
                                ColumnFamilyHandle* column_family) override { return nullptr; }

  // Returns iterators from a consistent database state across multiple column families.
  // Iterators are heap allocated and need to be deleted before the db is deleted.
  virtual Status NewIterators(const ReadOptions& options,
                              const std::vector<ColumnFamilyHandle*>& column_families,
                              std::vector<Iterator*>* iterators) NOOPE;

  // Returns the sequence number of the most recent transaction.
  virtual SequenceNumber GetLatestSequenceNumber() const override { return 0; }

  // Sets iter to an iterator that is positioned at a write-batch containing seq_number.
  // If the sequence number is non existent, it returns an iterator at the first available
  // seq_no after the requested seq_no Returns Status::OK if iterator is valid.
  // Must set WAL_ttl_seconds or WAL_size_limit_MB to large values to use this api, else the
  // WAL files will get cleared aggressively and the iterator might keep getting invalid before
  // an update is read.
  virtual Status GetUpdatesSince(SequenceNumber seq_number,
                                 unique_ptr<TransactionLogIterator>* iter,
                                 const TransactionLogIterator::ReadOptions&
                                 read_options = TransactionLogIterator::ReadOptions()) NOOPE;

  // =============================================================================================
  // SNAPSHOT METHODS
  // =============================================================================================

  // Return a handle to the current DB state.  Iterators created with this handle will all
  // observe a stable snapshot of the current DB state.  The caller must call
  // ReleaseSnapshot(result) when the snapshot is no longer needed.
  // nullptr will be returned if the DB fails to take a snapshot or does not support snapshot.
  virtual const Snapshot* GetSnapshot() override { return nullptr; }

  // Release a previously acquired snapshot.  The caller must not use snapshot after this call.
  virtual void ReleaseSnapshot(const Snapshot* snapshot) override {}

  // =============================================================================================
  // COLUMN FAMILY METHODS
  // =============================================================================================

  // Create a column_family and return the handle of column family through the argument handle.
  virtual Status CreateColumnFamily(const ColumnFamilyOptions& options,
                                    const std::string& column_family_name,
                                    ColumnFamilyHandle** handle) NOOPE;

  // Returns default column family.
  virtual ColumnFamilyHandle* DefaultColumnFamily() const override { return nullptr; }

  // Drop a column family specified by column_family handle. This call only records a drop
  // record in the manifest and prevents the column family from flushing and compacting.
  virtual Status DropColumnFamily(ColumnFamilyHandle* column_family) NOOPE;

  // Obtains the meta data of the specified column family of the DB. Status::NotFound() will be
  // returned if the current DB does not have any column family match the specified name.
  // If cf_name is not specified, then the metadata of the default column family will be returned.
  using DB::GetColumnFamilyMetaData;
  virtual void GetColumnFamilyMetaData(ColumnFamilyHandle* column_family,
                                       ColumnFamilyMetaData* metadata) override {}

  // =============================================================================================
  // PROPERTY METHODS
  // =============================================================================================

  // Like GetIntProperty(), but returns the aggregated int property from all column families.
  using DB::GetAggregatedIntProperty;
  virtual bool GetAggregatedIntProperty(const Slice& property,
                                        uint64_t* value) override { return false; }

  // Like GetProperty(), but only works for a subset of properties whose return value is an
  // integer. Return the value by integer. Supported properties:
  //  "rocksdb.num-immutable-mem-table"
  //  "rocksdb.mem-table-flush-pending"
  //  "rocksdb.compaction-pending"
  //  "rocksdb.background-errors"
  //  "rocksdb.cur-size-active-mem-table"
  //  "rocksdb.cur-size-all-mem-tables"
  //  "rocksdb.size-all-mem-tables"
  //  "rocksdb.num-entries-active-mem-table"
  //  "rocksdb.num-entries-imm-mem-tables"
  //  "rocksdb.num-deletes-active-mem-table"
  //  "rocksdb.num-deletes-imm-mem-tables"
  //  "rocksdb.estimate-num-keys"
  //  "rocksdb.estimate-table-readers-mem"
  //  "rocksdb.is-file-deletions-enabled"
  //  "rocksdb.num-snapshots"
  //  "rocksdb.oldest-snapshot-time"
  //  "rocksdb.num-live-versions"
  //  "rocksdb.current-super-version-number"
  //  "rocksdb.estimate-live-data-size"
  //  "rocksdb.total-sst-files-size"
  //  "rocksdb.base-level"
  //  "rocksdb.estimate-pending-compaction-bytes"
  //  "rocksdb.num-running-compactions"
  //  "rocksdb.num-running-flushes"
  using DB::GetIntProperty;
  virtual bool GetIntProperty(ColumnFamilyHandle* column_family, const Slice& property,
                              uint64_t* value) override { return false; }

  // DB implementations can export properties about their state via this method. If "property"
  // is a valid property understood by this DB implementation (see Properties struct above
  // for valid options), fills "*value" with its current value and returns true.
  // Otherwise, returns false.
  using DB::GetProperty;
  virtual bool GetProperty(ColumnFamilyHandle* column_family,
                           const Slice& property, std::string* value) override { return false; }

  // =============================================================================================
  // CONFIGURATION METHODS
  // =============================================================================================

  // Prevent file deletions. Compactions will continue to occur, but no obsolete files will be
  // deleted. Calling this multiple times have the same effect as calling it once.
  virtual Status DisableFileDeletions() NOOPE;

  // Enable automatic compactions for the given column families if they were
  // previously disabled. The function will first set the disable_auto_compactions option for
  // each column family to 'false', after which it will schedule a flush/compaction.
  // NOTE: Setting disable_auto_compactions to 'false' through SetOptions() API
  // does NOT schedule a flush/compaction afterwards, and only changes the
  // parameter itself within the column family option.
  virtual Status EnableAutoCompaction(const std::vector<ColumnFamilyHandle*>& handles) NOOPE;

  // Allow compactions to delete obsolete files.
  // If force == true, the call to EnableFileDeletions() will guarantee that file deletions are
  // enabled after the call, even if DisableFileDeletions() was called multiple times before.
  // If force == false, EnableFileDeletions will only enable file deletion after it's been
  // called at least as many times as DisableFileDeletions(), enabling the two methods to be
  // called by two threads concurrently without synchronization -- i.e., file deletions will
  // be enabled only after both threads call EnableFileDeletions().
  virtual Status EnableFileDeletions(bool force) NOOPE;

  // Sets the globally unique ID created at database creation time by invoking
  // Env::GenerateUniqueId(), in identity. Returns Status::OK if identity could be set properly.
  virtual Status GetDbIdentity(std::string& identity) const NOOPE;

  // Get Env object from the DB
  virtual Env* GetEnv() const override { return nullptr; }

  // Get DB name -- the exact same name that was provided as an argument to DB::Open().
  virtual const std::string& GetName() const override { return dbname; }

  // Return pointer to DB name -- same name that was provided as an argument to DB::Open().
  virtual const char* GetNamePtr() const { return dbname.c_str(); }

  // Get options in use.  During the process of opening the column family, the options
  // provided when calling DB::Open() or DB::CreateColumnFamily() will have been "sanitized"
  // and transformed in an implementation-defined manner.
  using DB::GetOptions;
  virtual const Options& GetOptions(ColumnFamilyHandle* column_family) const override {
    const Options* options = new Options();
    return *options;
  }
  using DB::GetDBOptions;
  virtual const DBOptions& GetDBOptions() const override { return dboptions; }

  // Number of files in level-0 that would stop writes.
  using DB::Level0StopWriteTrigger;
  virtual int Level0StopWriteTrigger(ColumnFamilyHandle* column_family) override { return 0; }

  // Maximum level to which a new compacted memtable is pushed if it does not create overlap.
  using DB::MaxMemCompactionLevel;
  virtual int MaxMemCompactionLevel(ColumnFamilyHandle* column_family) override { return 0; }

  // Number of levels used for this DB.
  using DB::NumberLevels;
  virtual int NumberLevels(ColumnFamilyHandle* column_family) override { return 0; }

  // Sets options in use by parsing string map provided.
  using DB::SetOptions;
  virtual Status SetOptions(ColumnFamilyHandle*,
                            const std::unordered_map<std::string, std::string>&) NOOPE;

  // =============================================================================================
  // STORAGE BACKEND METHODS
  // =============================================================================================

  // Load table file located at "file_path" into "column_family", a pointer to
  // ExternalSstFileInfo can be used instead of "file_path" to do a blind add that won't need
  // to read the file, move_file can be set to true to move the file instead of copying it.
  // (1) Key range in loaded table file can't overlap with existing keys or tombstones in DB.
  // (2) No other writes are allowed during AddFile call, otherwise DB may get corrupted.
  // (3) No snapshots are held.
  using DB::AddFile;
  virtual Status AddFile(ColumnFamilyHandle* column_family, const ExternalSstFileInfo* file_info,
                         bool move_file) NOOPE;
  virtual Status AddFile(ColumnFamilyHandle* column_family,
                         const std::string& file_path, bool move_file) NOOPE;

  // CompactFiles() inputs a list of files specified by file numbers and compacts them to the
  // specified level. Note that the behavior is different from CompactRange() in that this
  // performs the compaction job using the CURRENT thread.
  using DB::CompactFiles;
  virtual Status CompactFiles(const CompactionOptions& compact_options,
                              ColumnFamilyHandle* column_family,
                              const std::vector<std::string>& input_file_names,
                              const int output_level,
                              const int output_path_id = -1) NOOPE;

  // Compact the underlying storage for the key range [*begin,*end]. The actual compaction
  // interval might be superset of [*begin, *end]. In particular, deleted and overwritten
  // versions are discarded, and the data is rearranged to reduce the cost of operations
  // needed to access the data.  This operation should typically only be invoked by users who
  // understand the underlying implementation.
  //
  // begin==nullptr is treated as a key before all keys in the database.
  // end==nullptr is treated as a key after all keys in the database.
  // Therefore the following call will compact the entire database:
  //    db->CompactRange(options, nullptr, nullptr);
  //
  // Note that after the entire database is compacted, all data are pushed down to the last level
  // containing any data. If the total data size after compaction is reduced, that level might
  // not be appropriate for hosting all the files. In this case, client could set
  // options.change_level to true, to move the files back to the minimum level capable of
  // holding the data set or a given level (specified by non-negative options.target_level).
  using DB::CompactRange;
  virtual Status CompactRange(const CompactRangeOptions& options,
                              ColumnFamilyHandle* column_family,
                              const Slice* begin, const Slice* end) NOOPE;

  // Delete the file name from the db directory and update the internal state to reflect that.
  // Supports deletion of sst and log files only. 'name' must be path relative to the db
  // directory. eg. 000001.sst, /archive/000003.log
  virtual Status DeleteFile(std::string name) NOOPE;

  // Flush all mem-table data.
  using DB::Flush;
  virtual Status Flush(const FlushOptions& options, ColumnFamilyHandle* column_family) NOOPE;

  // For each i in [0,n-1], store in "sizes[i]", the approximate file system space used by
  // keys in "[range[i].start .. range[i].limit)". Note that the returned sizes measure file
  // system space usage, so if the user data compresses by a factor of ten, the returned
  // sizes will be one-tenth the size of the corresponding user data size. If include_memtable
  // is set to true, then the result will also include those recently written data in the
  // mem-tables if the mem-table type supports it.
  using DB::GetApproximateSizes;
  virtual void GetApproximateSizes(ColumnFamilyHandle* column_family,
                                   const Range* range, int n, uint64_t* sizes,
                                   bool include_memtable = false) override {}

  // Retrieve the list of all files in the database. The files are relative to the dbname and
  // are not absolute paths. The valid size of the manifest file is returned in
  // manifest_file_size. The manifest file is an ever growing file, but only the portion
  // specified by manifest_file_size is valid for this snapshot. Setting flush_memtable to true
  // does Flush before recording the live files. Setting flush_memtable to false is useful
  // when we don't want to wait for flush which may have to wait for compaction to complete
  // taking an indeterminate time. In case you have multiple column families, even if
  // flush_memtable is true, you still need to call GetSortedWalFiles after GetLiveFiles to
  // compensate for new data that arrived to already-flushed column families while other
  // column families were flushing.
  virtual Status GetLiveFiles(std::vector<std::string>&, uint64_t* manifest_file_size,
                              bool flush_memtable = true) NOOPE;

  // Returns a list of all table files with their level, start key and end key.
  virtual void GetLiveFilesMetaData(std::vector<LiveFileMetaData>*) override {}

  // Returns generic properties for tables in use.
  using DB::GetPropertiesOfAllTables;
  virtual Status GetPropertiesOfAllTables(ColumnFamilyHandle* column_family,
                                          TablePropertiesCollection* props) NOOPE;
  virtual Status GetPropertiesOfTablesInRange(ColumnFamilyHandle* column_family,
                                              const Range* range, std::size_t n,
                                              TablePropertiesCollection* props) NOOPE;

  // Retrieve the sorted list of all wal files with earliest file first.
  virtual Status GetSortedWalFiles(VectorLogPtr& files) NOOPE;

  // This function will wait until all currently running background processes finish. After it
  // returns, no background process will be run until UnblockBackgroundWork is called.
  virtual Status PauseBackgroundWork() NOOPE;
  virtual Status ContinueBackgroundWork() NOOPE;

  // Sync the wal. Note that Write() followed by SyncWAL() is not exactly the same as Write()
  // with sync=true: in the latter case the changes won't be visible until the sync is done.
  // Currently only works if allow_mmap_writes = false in Options.
  virtual Status SyncWAL() NOOPE;

protected:
  // Hide constructor, call Open() to create instead
  ScreeDB(const Options& options, const std::string& dbname);

private:
  ScreeDB(const ScreeDB&);                                               // prevent copying
  void operator=(const ScreeDB&);                                        // prevent assignment
  const std::string dbname;                                              // name when opened
  const DBOptions dboptions;                                             // options when opened
  ScreeDBTree* dbtree;                                                   // persistent tree
};

} // namespace screedb
} // namespace rocksdb
