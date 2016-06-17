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
#include <unistd.h>
#include "rocksdb/utilities/fptreedb.h"

#define LOG(msg) std::cout << "[FPTreeDB:" << GetName() << "] " << msg << "\n"

namespace rocksdb {

  // Open database using specified configuration options and name.
  Status FPTreeDB::Open(const Options& options, const FPTreeDBOptions& dboptions,
                        const std::string& dbname, FPTreeDB** dbptr) {
    FPTreeDB* impl = new FPTreeDB(options, dboptions, dbname);
    *dbptr = impl;
    return Status::OK();
  }

  // Default constructor.
  FPTreeDB::FPTreeDB(const Options& options, const FPTreeDBOptions& dboptions,
                     const std::string& dbname) : dbname_(dbname) {
    LOG("Opening database");

    // Open existing file if accessible, or create a new one
    if (access(GetNamePtr(), F_OK) != 0) {
      LOG("Creating new file");
      if ((pmem_pool_ = pmemobj_create(GetNamePtr(), POBJ_LAYOUT_NAME(FPTreeDB),
                                       PMEMOBJ_MIN_POOL, 0666)) == NULL) {
        perror("pmemobj_create");
        exit(1);
      }
      LOG("Created new file ok");
    } else {
      LOG("Opening existing file");
      if ((pmem_pool_ = pmemobj_open(GetNamePtr(), POBJ_LAYOUT_NAME(FPTreeDB))) == NULL) {
        perror("pmemobj_open");
        exit(1);
      }
      LOG("Opened existing file ok");
    }

    Recover(); // do initial recovery
    LOG("Opened database ok");
  }

  // Safely close the database.
  FPTreeDB::~FPTreeDB() {
    LOG("Closing database");
    pmemobj_close(pmem_pool_);
    LOG("Closed database ok");
  }

  // ===============================================================================================
  // KEY/VALUE METHODS
  // ===============================================================================================

  // Remove the database entry (if any) for "key".  Returns OK on success, and a non-OK status
  // on error.  It is not an error if "key" did not exist in the database.
  Status FPTreeDB::Delete(const WriteOptions& options, ColumnFamilyHandle* column_family,
                          const Slice& key) {
    /*
    ALGORITHM 5 - ConcurrentDelete(Key k)
     1: Decision = Result::Abort;
     2: while Decision == Result::Abort do
     3:    speculative_lock.acquire();
     4:    // PrevLeaf is locked only if Decision == LeafEmpty
     5:    (Leaf, PPrevLeaf) = FindLeafAndPrevLeaf(K);
     6:    if Leaf.lock == 1 then
     7:       Decision = Result::Abort; Continue;
     8:    if Leaf.Bitmap.count() == 1 then
     9:       if PPrevLeaf->lock == 1 then
    10:          Decision = Result::Abort; Continue;
    11:       Leaf.lock = 1; PPrevLeaf->lock = 1;
    12:       Decision = Result::LeafEmpty;
    13:    else
    14:       Leaf.lock = 1; Decision = Result::Delete;
    15:    speculative_lock.release();
    16: if Decision == Result::LeafEmpty then
    17:    DeleteLeaf(Leaf, PPrevLeaf);
    18:    PrevLeaf.lock = 0;
    19: else
    20:    slot = Leaf.FindInLeaf(K);
    21:    Leaf.Bitmap[slot] = 0; Persist(Leaf.Bitmap[slot]);
    22:    Leaf.lock = 0;
    */
    return Status::NotSupported();
  }

  // If the database contains an entry for "key" store the corresponding value in *value
  // and return OK. If there is no entry for "key" leave *value unchanged and return a status
  // for which Status::IsNotFound() returns true. May return some other Status on an error.
  Status FPTreeDB::Get(const ReadOptions& options, ColumnFamilyHandle* column_family,
                       const Slice& key, std::string* value) {
    /*
    ALGORITHM 1 - ConcurrentFind(Key k)
     1: while TRUE do
     2:    speculative_lock.acquire();
     3:    Leaf = FindLeaf(K);
     4:    if Leaf.lock == 1 then
     5:       speculative_lock.abort();
     6:       continue;
     7:    for each slot in Leaf do
     8:       set currentKey to key pointed to by Leaf.KV[slot].PKey
     9:       if Leaf.Bitmap[slot] == 1 and Leaf.Fingerprints[slot] == hash(K) and currentKey == K then
    10:          Val = Leaf.KV[slot].Val;
    11:          Break;
    12:    speculative_lock.release();
    13:    return Val;
    */
    return Status::NotSupported();
  }

  // Set the database entry for "key" to "value". If "key" already exists, it will be overwritten.
  // Returns OK on success, and a non-OK status on error.
  Status FPTreeDB::Put(const WriteOptions& options, ColumnFamilyHandle* column_family,
                       const Slice& key, const Slice& value) {
    /*
    ALGORITHM 8 - ConcurrentUpdate(Key k, Value v)
     1: Decision = Result::Abort;
     2: while Decision == Result::Abort do
     3:    speculative_lock.acquire();
     4:    (Decision, prevPos, Leaf, Parent) = FindKeyAndLockLeaf(K);
     5:    (Leaf, Parent) = FindLeaf(K);
     6:    if Leaf.lock == 1 then
     7:       Decision = Result::Abort; Continue;
     8:    Leaf.lock = 1;
     9:    prevPos = Leaf.FindKey(K);
    10:    Decision = Leaf.isFull() ? Result::Split : Result::Update;
    11:    speculative_lock.release();
    12: if Decision == Result::Split then
    13:    splitKey = SplitLeaf(Leaf);
    14: slot = Leaf.Bitmap.FindFirstZero();
    15: Leaf.KV[slot] = (K, V); Leaf.Fingerprints[slot] = hash(K);
    16: Persist(Leaf.KV[slot]); Persist(Leaf.Fingerprints[slot]);
    17: copy Leaf.Bitmap in tmpBitmap;
    18: tmpBitmap[prevSlot] = 0; tmpBitmap[slot] = 1;
    19: Leaf.Bitmap = tmpBitmap; Persist(Leaf.Bitmap);
    20: if Decision == Result::Split then
    21:    speculative_lock.acquire();
    22:    UpdateParents(splitKey, Parent, Leaf);
    23:    speculative_lock.release();
    24: Leaf.lock = 0;
    */
    return Status::NotSupported();
  }

  // ===============================================================================================
  // PROTECTED LEAF METHODS
  // ===============================================================================================

  void FPTreeDB::DeleteLeaf() {
    /*
    ALGORITHM 6 - DeleteLeaf(LeafNode Leaf, LeafNode PPrevLeaf)
     1: get the head of the linked list of leaves PHead
     2: get μLog from DeleteLogQueue;
     3: set μLog.PCurrentLeaf to persistent address of Leaf;
     4: Persist(μLog.PCurrentLeaf);
     5: if μLog.PCurrentLeaf == PHead then
     6:    // Leaf is the head of the linked list of leaves
     7:    PHead = Leaf.Next;
     8:    Persist(PHead);
     9: else
    10:    μLog.PPrevLeaf = PPrevLeaf;
    11:    Persist(μLog.PPrevLeaf);
    12:    PrevLeaf.Next = Leaf.Next;
    13:    Persist(PrevLeaf.Next);
    14: Deallocate(μLog.PCurrentLeaf);
    15: reset μLog;
   */
  }

  void FPTreeDB::FindLeaf(const Slice& key) {

  }

  void FPTreeDB::FindLeafAndPrevLeaf(const Slice& key) {

  }

  void FPTreeDB::SplitLeaf() {
    /*
    ALGORITHM 3 - SplitLeaf(LeafNode Leaf)
     1: get μLog from SplitLogQueue;
     2: set μLog.PCurrentLeaf to persistent address of Leaf;
     3: Persist(μLog.PCurrentLeaf);
     4: Allocate(μLog.PNewLeaf, sizeof(LeafNode))
     5: set NewLeaf to leaf pointed to by μLog.PNewLeaf;
     6: Copy the content of Leaf into NewLeaf;
     7: Persist(NewLeaf);
     8: (splitKey, bmp) = FindSplitKey(Leaf);
     9: NewLeaf.Bitmap = bmp;
    10: Persist(NewLeaf.Bitmap);
    11: Leaf.Bitmap = inverse(NewLeaf.Bitmap);
    12: Persist(Leaf.Bitmap);
    13: set Leaf.Next to persistent address of NewLeaf;
    14: Persist(Leaf.Next);
    15: reset μLog;
    */
  }

  // ===============================================================================================
  // PROTECTED RECOVERY METHODS
  // ===============================================================================================

  void FPTreeDB::Recover() {
    /*
    ALGORITHM 9 - Recover()
     1: if Tree.Status == NotInitialized then
     2:    Tree.init();
     3: else
     4:    for each SplitLog in Tree.SplitLogArray do
     5:       RecoverSplit(SplitLog);
     6:    for each DeleteLog in Tree.DeleteLogArray do
     7:       RecoverDelete(DeleteLog);
     8: RebuildInnerNodes();
     9: RebuildLogQueues();
    */
    LOG("Recovering database");
    RebuildInnerNodes();
    RebuildLogQueues();
    LOG("Recovered database ok");
  }

  void FPTreeDB::RecoverDelete() {
    /*
    ALGORITHM 7 - RecoverDelete(DeleteLog μLog)
     1: get head of linked list of leaves PHead;
     2: if μLog.PCurrentLeaf != NULL and μLog.PPrevLeaf != NULL then
     3:    // Crashed between lines DeleteLeaf:12-14
     4:    Continue from DeleteLeaf:12;
     5: else
     6:    if μLog.PCurrentLeaf != NULL and μLog.PCurrentLeaf == PHead then
     7:       // Crashed at line DeleteLeaf:7
     8:       Continue from DeleteLeaf:7;
     9:    else
    10:       if μLog.PCurrentLeaf != NULL and μLog.PCurrentLeaf→Next == PHead then
    11:          // Crashed at line DeleteLeaf:14
    12:          Continue from DeleteLeaf:14;
    13:       else
    14:          reset μLog;
    */
  }

  void FPTreeDB::RecoverSplit() {
    /*
    ALGORITHM 4 - RecoverSplit(SplitLog μLog)
     1: if μLog.PCurrentLeaf == NULL then
     2:    return;
     3: if μLog.PNewLeaf == NULL then
     4:    // Crashed before SplitLeaf:4
     5:    reset μLog;
     6: else
     7:    if μLog.PCurrentLeaf.Bitmap.IsFull() then
     8:       // Crashed before SplitLeaf:11
     9:       Continue leaf split from SplitLeaf:6;
    10:    else
    11:       // Crashed after SplitLeaf:11
    12:       Continue leaf split from SplitLeaf:11;
    */
  }

  void FPTreeDB::RebuildInnerNodes() {
    LOG("Rebuilding inner nodes");
    LOG("Rebuilt inner nodes ok");
  }

  void FPTreeDB::RebuildLogQueues() {
    LOG("Rebuilding log queues");
    LOG("Rebuilt log queues ok");
  }

}  // namespace rocksdb

#endif  // ROCKSDB_LITE
