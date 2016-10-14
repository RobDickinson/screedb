ScreeDB
=======

**RocksDB Without LSM or Block Devices**

ScreeDB is a RocksDB utility that bypasses the RocksDB LSM implementation entirely, rather than adding or optimizing memtable/table types within the LSM implementation. ScreeDB uses NVM exclusively (without mixing other types of persistent storage) and targets media sizes and latencies expected for Crystal Ridge. ScreeDB uses NVML (from pmem.io) to enable use of persistent memory and to bypass the Linux page cache and filesystem layers.

As a utility, ScreeDB does not modify the core RocksDB distribution, but only adds code at expected extension points. The structure of extensions for ScreeDB takes inspiration from existing SpatialDB and TransactionDB utilities, which provide high-level wrappers using the RocksDB API.

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
-	utilities/screedb/screedb_stress.cc (stress tests using large dataset)
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

**cpp_map**

Use of NVML C++ bindings by ScreeDB was lifted from this example program. Many thanks to Tomasz Kapela for providing a great example to follow!

**KV/NVM pathfinding**

This research explores LSM optimizations in the context of an internal RocksDB fork.

```
Sign up on gerrit server at http://az-sg-sw01.ch.intel.com/gerrit
**Warning!** HTTP proxy may need to be disabled on Ubuntu to connect to gerrit web UI

git clone -b dev/kv_pathfinding ssh://az-sg-sw01.ch.intel.com:29418/rocksdb rocksdb.kv
```
