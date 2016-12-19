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
#include <algorithm>
#include "screedb.h"

#define DO_LOG 0
#define LOG(msg) if (DO_LOG) std::cout << "[ScreeDB:" << GetName() << "] " << msg << "\n"

namespace rocksdb {
namespace screedb {

// Static factory for RocksDB-compatible persistent trees
Status ScreeDB::Open(const Options& options, const std::string& dbname, ScreeDB** dbptr) {
  *dbptr = new ScreeDB(options, dbname);
  return Status::OK();
}

// Construct a RocksDB-compatible persistent tree
ScreeDB::ScreeDB(const Options& options, const std::string& name)
        : dbname(name), dboptions(options) { dbtree = new ScreeDBTree(dbname); }

// Safely free a RocksDB-compatible persistent tree
ScreeDB::~ScreeDB() { delete dbtree; }

// Construct a persistent tree
ScreeDBTree::ScreeDBTree(const std::string& name) : name(name) {
  LOG("Opening persistent tree");
  if (access(GetNamePtr(), F_OK) != 0) {
    LOG("Creating pool");
    pop_ = pool<ScreeDBRoot>::create(GetNamePtr(), "ScreeDB",            // todo name is hardcoded
                                     PMEMOBJ_MIN_POOL * 450, S_IRWXU);   // todo size is hardcoded
  } else {
    pop_ = pool<ScreeDBRoot>::open(GetNamePtr(), "ScreeDB");             // todo name is hardcoded
  }
  Recover();
  LOG("Opened tree ok");
}

// Safely free a persistent tree
ScreeDBTree::~ScreeDBTree() {
  LOG("Closing tree");
  Shutdown();
  pop_.close();
  LOG("Closed tree ok");
}

// ===============================================================================================
// KEY/VALUE METHODS
// ===============================================================================================

// Remove the database entry (if any) for "key".  Returns OK on success, and a non-OK status
// on error.  It is not an error if "key" did not exist in the database.
Status ScreeDBTree::Delete(const Slice& key) {
  LOG("Delete key=" << key.data_);
  auto leafnode = LeafSearch(key);
  if (!leafnode) {
    LOG("   head not present");
    return Status::OK();
  }
  const uint8_t hash = PearsonHash(key.data_, key.size_);
  for (int slot = NODE_KEYS; slot--;) {
    if (leafnode->hashes[slot] == hash) {
      auto leaf = leafnode->leaf;
      if (strcmp(leaf->kv_keys[slot].get_ro().data(), key.data_) == 0) {
        LOG("   freeing slot=" << slot);
        leafnode->hashes[slot] = 0;
        transaction::exec_tx(pop_, [&] {
          leaf->hashes[slot] = 0;
        });
        break;  // no duplicate keys allowed
      }
    }
  }
  return Status::OK();
}

// If the database contains an entry for "key" store the corresponding value in *value
// and return OK. If there is no entry for "key" leave *value unchanged and return a status
// for which Status::IsNotFound() returns true. May return some other Status on an error.
Status ScreeDBTree::Get(const Slice& key, std::string* value) {
  LOG("Get key=" << key.data_);
  auto leafnode = LeafSearch(key);
  if (!leafnode) {
    LOG("   head not present");
    return Status::NotFound();
  }
  const uint8_t hash = PearsonHash(key.data_, key.size_);
  for (int slot = NODE_KEYS; slot--;) {
    if (leafnode->hashes[slot] == hash) {
      auto leaf = leafnode->leaf;
      if (strcmp(leaf->kv_keys[slot].get_ro().data(), key.data_) == 0) {
        value->append(leaf->kv_values[slot].get_ro().data());
        LOG("   found value=" << *value << ", slot=" << slot);
        return Status::OK();
      }
    }
  }
  LOG("   could not find key");
  return Status::NotFound();
}

// If keys[i] does not exist in the database, then the i'th returned status will be one for
// which Status::IsNotFound() is true, and (*values)[i] will be set to some arbitrary value
// (often ""). Otherwise, the i'th returned status will have Status::ok() true, and
// (*values)[i] will store the value associated with keys[i].
// (*values) will always be resized to be the same size as (keys).
// Similarly, the number of returned statuses will be the number of keys.
// Note: keys will not be "de-duplicated". Duplicate keys will return duplicate values in order.
std::vector<Status> ScreeDBTree::MultiGet(const std::vector<Slice>& keys,
                                          std::vector<std::string>* values) {
  LOG("MultiGet for " << keys.size() << " keys");
  std::vector<Status> status = std::vector<Status>();
  for (auto& key: keys) {
    std::string value;
    Status s = Get(key.data_, &value);
    status.push_back(s);
    values->push_back(s.ok() ? value : "");
  }
  LOG("MultiGet done for " << keys.size() << " keys");
  return status;
}

// Set the database entry for "key" to "value". If "key" already exists, it will be overwritten.
// Returns OK on success, and a non-OK status on error.
Status ScreeDBTree::Put(const Slice& key, const Slice& value) {
  LOG("Put key=" << key.data_ << ", value=" << value.data_);
  const uint8_t hash = PearsonHash(key.data_, key.size_);

  // add head leaf if none present
  auto leafnode = LeafSearch(key);
  if (!leafnode) {
    LOG("   adding head leaf");
    leafnode = new ScreeDBLeafNode();
    leafnode->is_leaf = true;
    persistent_ptr<ScreeDBLeaf> new_leaf;
    auto root = pop_.get_root();
    auto old_head = root->head;
    transaction::exec_tx(pop_, [&] {
      new_leaf = make_persistent<ScreeDBLeaf>();
      new_leaf->next = old_head;
      leafnode->leaf = new_leaf;
      LeafFillSpecificSlot(leafnode, hash, key, value, 0);
      root->head = new_leaf;
    });
    top_ = leafnode;
    return Status::OK();
  }

  // update leaf, splitting if necessary
  if (LeafFillSlotForKey(leafnode, hash, key, value)) {
    return Status::OK();
  } else {
    LeafSplit(leafnode, hash, key, value);
    return Status::OK();
  }
}

// ===============================================================================================
// PROTECTED LEAF METHODS
// ===============================================================================================

void ScreeDBTree::LeafDebugDump(ScreeDBNode* node) {
  if (DO_LOG) {
    if (node->is_leaf) {
      auto leaf = ((ScreeDBLeafNode*) node)->leaf;
      for (int slot = 0; slot < NODE_KEYS; slot++) {
        LOG("      " << std::to_string(slot) << "="
                     << (leaf->hashes[slot] == 0 ? "n/a" : leaf->kv_keys[slot].get_ro().data()));
      }
    } else {
      ScreeDBInnerNode* inner_node = (ScreeDBInnerNode*) node;
      LOG("      keycount: " << std::to_string(inner_node->keycount));
      for (int idx = 0; idx < inner_node->keycount; idx++) {
        LOG("      " << std::to_string(idx) << ":" << inner_node->keys[idx]);
      }
    }
  }
}

void ScreeDBTree::LeafDebugDumpWithChildren(ScreeDBInnerNode* inner) {
  LeafDebugDump(inner);
  for (int i = 0; i < inner->keycount + 1; i++) {
    LOG ("      dumping child node " << std::to_string(i) << "------------------------");
    LeafDebugDump(inner->children[i]);
  }
}

void ScreeDBTree::LeafFillFirstEmptySlot(ScreeDBLeafNode* leafnode, const uint8_t hash,
                                         const Slice& key, const Slice& value) {
  for (int slot = NODE_KEYS; slot--;) {
    if (leafnode->hashes[slot] == 0) {
      LeafFillSpecificSlot(leafnode, hash, key, value, slot);
      return;
    }
  }
}

bool ScreeDBTree::LeafFillSlotForKey(ScreeDBLeafNode* leafnode, const uint8_t hash,
                                     const Slice& key, const Slice& value) {
  // scan for empty/matching slots
  int last_empty_slot = -1;
  int key_match_slot = -1;
  for (int slot = NODE_KEYS; slot--;) {
    auto slot_hash = leafnode->hashes[slot];
    if (slot_hash == 0) {
      last_empty_slot = slot;
    } else if (slot_hash == hash) {
      if (strcmp(leafnode->leaf->kv_keys[slot].get_ro().data(), key.data_) == 0) {
        key_match_slot = slot;
        break;  // no duplicate keys allowed
      }
    }
  }

  // update suitable slot if found
  int slot = key_match_slot >= 0 ? key_match_slot : last_empty_slot;
  if (slot >= 0) {
    LOG("   filling slot=" << slot);
    transaction::exec_tx(pop_, [&] {
      LeafFillSpecificSlot(leafnode, hash, key, value, slot);
    });
  }
  return slot >= 0;
}

void ScreeDBTree::LeafFillSpecificSlot(ScreeDBLeafNode* leafnode, const uint8_t hash,
                                       const Slice& key, const Slice& value, const int slot) {
  auto leaf = leafnode->leaf;
  if (leafnode->hashes[slot] == 0) leaf->kv_keys[slot].get_rw().set(key.data_);
  leafnode->hashes[slot] = hash;
  leaf->hashes[slot] = hash;
  leaf->kv_values[slot].get_rw().set(value.data_);
}

ScreeDBLeafNode* ScreeDBTree::LeafSearch(const Slice& key) {
  ScreeDBNode* node = top_;
  if (node == nullptr) return nullptr;
  bool matched;
  while (!node->is_leaf) {
    matched = false;
    ScreeDBInnerNode* inner = (ScreeDBInnerNode*) node;
    const uint8_t keycount = inner->keycount;
    for (uint8_t idx = 0; idx < keycount; idx++) {
      node = inner->children[idx];
      if (strcmp(key.data_, inner->keys[idx].c_str()) <= 0) {
        matched = true;
        break;
      }
    }
    if (!matched) node = inner->children[keycount];
  }
  return (ScreeDBLeafNode*) node;
}

void ScreeDBTree::LeafSplit(ScreeDBLeafNode* leafnode, const uint8_t hash,
                            const Slice& key, const Slice& value) {
  const auto leaf = leafnode->leaf;
  const char* keys[NODE_KEYS + 1];                                       // temp array for sort
  for (int slot = NODE_KEYS; slot--;) {                                  // iterate leaf slots
    keys[slot] = leaf->kv_keys[slot].get_ro().data();                    // shallow pointer copy
  }                                                                      // done iterating
  keys[NODE_KEYS] = key.data_;                                           // copy new key pointer
  std::sort(std::begin(keys), std::end(keys),                            // sort the key array
            [](const char*& lhs, const char*& rhs) {                     // using closure method
              return (strcmp(lhs, rhs) < 0);                             // comparing as c-strings
            });                                                          // done with closure
  std::string split_key = keys[NODE_KEYS_MIDPOINT];                      // read from the middle
  LOG("   splitting leaf at key=" << split_key);

  // split leaf into two leaves, moving slots that sort above split key to new leaf
  auto new_leafnode = new ScreeDBLeafNode();
  new_leafnode->parent = leafnode->parent;
  new_leafnode->is_leaf = true;
  persistent_ptr<ScreeDBLeaf> new_leaf;
  auto root = pop_.get_root();
  auto old_head = root->head;
  transaction::exec_tx(pop_, [&] {
    new_leaf = make_persistent<ScreeDBLeaf>();
    new_leaf->next = old_head;
    new_leafnode->leaf = new_leaf;
    for (int slot = NODE_KEYS; slot--;) {
      const char* slot_key = leaf->kv_keys[slot].get_ro().data();
      if (strcmp(slot_key, split_key.data()) > 0) {
        new_leaf->kv_keys[slot].get_rw().set(slot_key);  // todo use copy-vs-swap here too
        new_leafnode->hashes[slot] = leafnode->hashes[slot];
        new_leaf->hashes[slot] = leafnode->hashes[slot];
        leafnode->hashes[slot] = 0;
        leaf->hashes[slot] = 0;
        if (strlen(leaf->kv_values[slot].get_ro().data()) <= SSO_CHARS) {
          new_leaf->kv_values[slot].get_rw().set(leaf->kv_values[slot].get_ro().data());
        } else {
          new_leaf->kv_values[slot].swap(leaf->kv_values[slot]);
        }
      }
    }
    auto target = strcmp(key.data_, split_key.data()) > 0 ? new_leafnode : leafnode;
    LeafFillFirstEmptySlot(target, hash, key, value);
    root->head = new_leaf;
  });

  // recursively update volatile parents outside persistent transaction
  LeafUpdateParentsAfterSplit(leafnode, new_leafnode, &split_key);
}

void ScreeDBTree::LeafUpdateParentsAfterSplit(ScreeDBNode* node, ScreeDBNode* new_node,
                                              std::string* split_key) {
  if (!node->parent) {
    LOG("   creating new top node for split_key=" << *split_key);
    auto top = new ScreeDBInnerNode();
    top->keycount = 1;
    top->keys[0] = *split_key;
    top->children[0] = node;
    top->children[1] = new_node;
    node->parent = top;
    new_node->parent = top;
    LeafDebugDumpWithChildren(top);                                      // dump details
    top_ = top;                                                          // assign new top node
    return;                                                              // end recursion
  }

  LOG("   updating parents for split_key=" << *split_key);
  ScreeDBInnerNode* inner = (ScreeDBInnerNode*) node->parent;
  { // insert split_key and new_node into inner node in sorted order
    const uint8_t keycount = inner->keycount;
    int idx = 0;  // position where split_key should be inserted
    while (idx < keycount && inner->keys[idx].compare(*split_key) <= 0) idx++;
    for (int i = keycount - 1; i >= idx; i--) inner->keys[i + 1] = inner->keys[i];
    for (int i = keycount; i >= idx; i--) inner->children[i + 1] = inner->children[i];
    inner->keys[idx] = *split_key;
    inner->children[idx + 1] = new_node;
    inner->keycount = (uint8_t) (keycount + 1);
  }
  const uint8_t keycount = inner->keycount;
  if (keycount <= INNER_KEYS) return;                                    // end recursion

  // split inner node at the midpoint, update parents as needed
  auto new_inner = new ScreeDBInnerNode();                               // allocate new node
  new_inner->parent = inner->parent;                                     // set parent reference
  for (int i = INNER_KEYS_UPPER; i < keycount; i++) {                    // copy all upper keys
    new_inner->keys[i - INNER_KEYS_UPPER] = inner->keys[i];              // copy key string
  }
  for (int i = INNER_KEYS_UPPER; i < keycount + 1; i++) {                // copy all upper children
    new_inner->children[i - INNER_KEYS_UPPER] = inner->children[i];      // copy child reference
    new_inner->children[i - INNER_KEYS_UPPER]->parent = new_inner;       // set parent reference
  }
  new_inner->keycount = INNER_KEYS_MIDPOINT;                             // always half the keys
  std::string new_split_key = inner->keys[INNER_KEYS_MIDPOINT];          // save for recursion
  inner->keycount = INNER_KEYS_MIDPOINT;                                 // half of keys remain
  LeafUpdateParentsAfterSplit(inner, new_inner, &new_split_key);         // recursive update
}

// ===============================================================================================
// PROTECTED LIFECYCLE METHODS
// ===============================================================================================

void ScreeDBTree::Recover() {
  LOG("Recovering tree");
  auto root = pop_.get_root();
  if (!root->head) {
    LOG("   creating root");
    transaction::exec_tx(pop_, [&] {
      root->opened = 1;
      root->closed = 0;
    });
  } else {
    LOG("   recovering head: opened=" << root->opened << ", closed=" << root->closed);
    // @todo handle opened/closed inequality, including count correction
    RebuildNodes();
    transaction::exec_tx(pop_, [&] { root->opened = root->opened + 1; });
  }
  LOG("Recovered tree ok");
}

void ScreeDBTree::RebuildNodes() {
  LOG("   rebuilding nodes");

  // traverse persistent leaves to build volatile leaf nodes
  ScreeDBLeafNode* first_leafnode = nullptr;
  auto leaf = pop_.get_root()->head;
  while (true) {  // todo unbounded loop
    ScreeDBLeafNode* leafnode = new ScreeDBLeafNode();
    leafnode->leaf = leaf;
    leafnode->is_leaf = true;
    for (int slot = NODE_KEYS; slot--;) leafnode->hashes[slot] = leaf->hashes[slot];
    if (first_leafnode == nullptr) first_leafnode = leafnode;
    if (leaf->next) {
      leaf = leaf->next;
    } else {
      break;
    }
  }

  // build inner nodes and initialize top pointer
  {
    top_ = first_leafnode;  // todo should be the topmost inner node
  }

  LOG("   rebuilt nodes ok");
}

void ScreeDBTree::Shutdown() {
  LOG("Shutting down tree");
  auto root = pop_.get_root();
  transaction::exec_tx(pop_, [&] { root->closed = root->closed + 1; });
  LOG("Shut down tree ok");
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

// Modified Pearson hashing algorithm from RFC 3074
uint8_t ScreeDBTree::PearsonHash(const char* data, const size_t size) {
  uint8_t hash = (uint8_t) size;
  for (size_t i = size; i > 0;) {  // todo first n chars instead?
    hash = PEARSON_LOOKUP_TABLE[hash ^ data[--i]];
  }

  // MODIFICATION START
  if (hash == 0) hash = 1;  // never return 0, this is reserved for "null"
  // MODIFICATION END

  return hash;
}

// ===============================================================================================
// STRING CLASS METHODS
// ===============================================================================================

char* ScreeDBString::data() const {
  return str ? str.get() : const_cast<char*>(sso);                       // return short or long
}

void ScreeDBString::set(const Slice& slice) {
  if (slice.size_ <= SSO_CHARS) {                                        // setting short value?
    if (str) {                                                           // value already present?
      delete_persistent<char[]>(str, strlen(str.get()) + 1);             // free value memory
      str = nullptr;                                                     // zero out pointer
    }
    pmemobj_tx_add_range_direct(sso, SSO_SIZE);                          // add sso buffer to txn
    strcpy(sso, slice.data_);                                            // copy slice data
  } else {                                                               // setting long value?
    if (str) delete_persistent<char[]>(str, strlen(str.get()) + 1);      // free value if present
    str = make_persistent<char[]>(slice.size_ + 1);                      // allocate value pmem
    strcpy(str.get(), slice.data_);                                      // copy slice data
  }
}

} // namespace screedb
} // namespace rocksdb
