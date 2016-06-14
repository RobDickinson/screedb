FPTreeDB
========

**Fingerprinted Persistent Trees for RocksDB**

FPTreeDB is a RocksDB utility whose name and design is inspired by the paper "FPTree: A Hybrid SCM-DRAM Persistent and Concurrent B-Tree for Storage Class Memory."

FPTreeDB bypasses the RocksDB LSM implementation entirely, as opposed to adding or optimizing memtable/table types within the existing LSM implementation. FPTreeDB uses the AppDirect programming model (specifically NVML's libpmemobj library) to also bypass the Linux page cache and filesystem, which are known to incur significant overhead for RocksDB workloads.

As a utility, FPTreeDB does not modify the core RocksDB distribution, but only adds code at expected extension points. The structure of these extensions for FPTreeDB takes inspiration from existing SpatialDB and TransactionDB utilities, which provide high-level wrappers using the RocksDB API.

Contents
--------

<ul>
<li><a href="#project_structure">Project Structure</a></li>
<li><a href="#installation">Installation</a></li>
<li><a href="#building_and_running_tests">Building and Running Tests</a></li>
<li><a href="#configuring_clion_project">Configuring CLion Project</a></li>
</ul>

<a name="project_structure"/>

Project Structure
-----------------

This project is based on RocksDB 4.6.1, which is a stable public release.

New files added:

-	include/rocksdb/utilities/fptreedb.h (declares FPTreeDB class)
-	utilities/fptreedb/fptreedb.cc (FPTreeDB class implementation)
-	examples/fptreedb_example.cc (test program adapted from simple_example)

Existing files modified:

-	src.mk (to include FPTreeDB in static library)
-	examples/.gitignore (to ignore fptreedb_example)
-	examples/Makefile (to build fptreedb_example)

<a name="installation"/>

Installation
------------

Start with Ubuntu 16.04 (either desktop or server distribution) or other 64-bit Linux distribution. OSX and Windows are not supported by the Intel NVM library, so don't use those.

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
git clone https://github.com/RobDickinson/fptreedb.git
```

<a name="building_and_running_tests"/>

Building and Running Tests
--------------------------

vi ~/fptreedb-env.sh, add:

```
#!/bin/bash -e
export FPTREEDB_HOME=$HOME/fptreedb
```

vi ~/fptreedb-test.sh, add:

```
#!/bin/bash -e
rm -rf /tmp/fptreedb_example
pushd . > /dev/null
cd $FPTREEDB_HOME
make static_lib -j8
cd examples
make fptreedb_example
./fptreedb_example
popd > /dev/null
```

To run tests:

```
chmod +x ~/fptreedb*
source ~/fptreedb-env.sh
~/fptreedb-test.sh
```

To run tests on underlying RocksDB distribution:

```
source ~/fptreedb-env.sh
cd $FPTREEDB_HOME
make -j8                    # build everything
make check                  # run RocksDB tests
make check V=1              # use verbose mode if something fails
make clean                  # clean up afterwards
```

<a name="configuring_clion_project"/>

Configuring CLion Project
-------------------------

Obviously the use of an IDE is a personal preference -- CLion is not required for FPTreeDB development (it's not free), but it's very easy to configure if you have a valid license.

If prompted for Toolchain configuration, choose bundled CMake.

Use wizard to create project:

-	From Welcome screen, select "Import Project From Sources"
-	select FPTREEDB_HOME directory
-	select "Overwrite CMakeLists.txt"
-	deselect all directories, then select db

When the project opens, add these lines to CMakeLists.txt:

```
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/third-party/gtest-1.7.0/fused-src)
```

In Project View, right-click on db|examples|include|memtable|table|util|utilities directories and select Mark Directory As | Project Sources and Headers. Wait for CLion to finish indexing, and you're good to go!

The .gitignore for this project ignores CMakeLists.txt and the entire .idea directory.
