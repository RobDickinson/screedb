ScreeDB
=======

**RocksDB Without LSM or Block Devices**

ScreeDB is a RocksDB utility that bypasses the RocksDB LSM implementation entirely, rather than adding or optimizing memtable/table types within the LSM implementation. ScreeDB uses NVM exclusively (without mixing other types of persistent storage) and targets media sizes and latencies expected for Crystal Ridge. ScreeDB uses NVML (from pmem.io) to enable use of persistent memory and to bypass the Linux page cache and filesystem layers.

As a utility, ScreeDB does not modify the core RocksDB distribution, but only adds code at expected extension points. The structure of extensions for ScreeDB takes inspiration from existing SpatialDB and TransactionDB utilities, which provide high-level wrappers using the RocksDB API.

*This project is no longer being maintained, instead refer to [pmemkv](https://github.com/RobDickinson/pmemkv). This
is experimental pre-release software and should not be used in production systems. This has known memory leaks and
other issues that we don't intend to ever fix.*

Contents
--------

<ul>
<li><a href="#project_structure">Project Structure</a></li>
<li><a href="#installation">Installation</a></li>
<li><a href="#configuring_clion_project">Configuring CLion Project</a></li>
<li><a href="#related_work">Related Work</a></li>
</ul>

<a name="project_structure"/>

Project Structure
-----------------

This project is based on RocksDB 4.6.1, which is a stable public release. Files were added to this base distribution but no existing files from RocksDB were modified.

New files added:

-	utilities/screedb/screedb.h (class header)
-	utilities/screedb/screedb.cc (class implementation)
-	utilities/screedb/screedb_example.cc (small example adapted from simple_example)
-	utilities/screedb/screedb_stress_rocks.cc (stress tests using RocksDB API)
-	utilities/screedb/screedb_stress_tree.cc (stress tests using persistent tree API)
-	utilities/screedb/screedb_test.cc (unit tests using Google C++ Testing Framework)

<a name="installation"/>

Installation
------------

Start with Ubuntu 16.04 (either desktop or server distribution) or other 64-bit Linux distribution. OSX and Windows are not supported by NVML, so don't use those.

Install RocksDB required libraries:

-	https://github.com/facebook/rocksdb/blob/master/INSTALL.md#supported-platforms

Install NVML:

```
cd ~
git clone https://github.com/pmem/nvml.git
cd nvml
sudo make install -j8
```

Get the sources:

```
cd ~
git clone https://github.com/RobDickinson/screedb.git
```

Run the tests:

```
cd screedb
make static_lib -j8            # build RocksDB distribution
cd utilities/screedb
make                           # build and run ScreeDB tests
```

<a name="configuring_clion_project"/>

Configuring CLion Project
-------------------------

Obviously the use of an IDE is a personal preference -- CLion is not required for ScreeDB development (it's not free), but it's very easy to configure if you have a valid license.

If prompted for Toolchain configuration, choose bundled CMake.

Use wizard to create project:

-	From Welcome screen, select "Import Project From Sources"
-	select root directory
-	select "Overwrite CMakeLists.txt"
-	deselect all directories, then select db

When the project opens, add these lines to CMakeLists.txt:

```
include_directories(/usr/local/include/libpmemobj)
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/third-party/gtest-1.7.0/fused-src)
```

In Project View, right-click on db|examples|include|memtable|table|util|utilities directories and select Mark Directory As | Project Sources and Headers. Wait for CLion to finish indexing, and you're good to go!

The .gitignore for this project ignores CMakeLists.txt and the entire .idea directory.

Apply editor/beautifier settings (matching RocksDB code style) by importing clion-settings.jar from clion-settings.zip.

<a name="related_work"/>

Related Work
------------

**FPTree**

This research paper describes a hybrid dram/pmem tree design (similar to ScreeDB) but doesn't provide any code, and even in describing the design omits certain important implementation details.

Beyond providing a clean-room implementation, the design of ScreeDB differs from FPTree in several important areas:

1. ScreeDB has the goal of RocksDB interoperability, and so its implementation heavily relies on the RocksDB API and its primitive types (like Slice). FPTree does not target RocksDB compatibility.

2. FPTree does not specify a hash method implementation, where ScreeDB uses a Pearson hash (RFC 3074).

3. Within its persistent leaves, FPTree uses an array of key hashes with a separate visibility bitmap to track what hash slots are occupied. ScreeDB takes a different approach and uses only an array of key hashes (no bitmaps).  ScreeDB relies on a specially modified Pearson hash function, where a hash value of zero always indicates the slot is unused by convention. This optimization eliminates the cost of using and maintaining visibility bitmaps as well as cramming more hashes into a single cache-line, and affects the implementation of every primitive operation in the tree.

4. ScreeDB additionally caches this array of key hashes in DRAM (in addition to storing as part of the persistent leaf). This speeds leaf operations, especially with slower media, for what seems like an acceptable rise in DRAM usage.

5. ScreeDB is written using NVML C++ bindings, which exerts influence on its design and implementation. ScreeDB uses generic NVML transactions (ie. transaction::exec_tx() closures), there is no need for micro-logging structures as described in the FPTree paper to make internal delete and split operations safe. Preallocation of relatively large leaf objects (an important facet of the FPTree design) is slower when using NVML then allocating objects immediately when they are needed, so ScreeDB intentionally avoids this design pattern. ScreeDB also adjusts sizes of data structures (conforming specifically to NVML primitive types) for best cache-line optimization.

6. The design of ScreeDB assumes use of an asynchronous garbage collector thread, which is not yet implemented but is necessary to reclaim space after delete & split operations. FPTree has no garbage collector.

**cpp_map**

Use of NVML C++ bindings by ScreeDB was lifted from this example program. Many thanks to Tomasz Kapela for providing a great example to follow!

**KV/NVM pathfinding**

This research explores LSM optimizations in the context of an internal RocksDB fork.

```
Sign up on gerrit server at http://az-sg-sw01.ch.intel.com/gerrit
**Warning!** HTTP proxy may need to be disabled on Ubuntu to connect to gerrit web UI

git clone -b dev/kv_pathfinding ssh://az-sg-sw01.ch.intel.com:29418/rocksdb rocksdb.kv
```
