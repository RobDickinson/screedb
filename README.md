fptreedb
========

Fingerprinted Persistent Trees for RocksDB

(executive summary goes here)

Contents
--------

<ul>
<li><a href="#getting_started">Getting Started</a></li>
<li><a href="#building_and_running_tests">Building and Running Tests</a></li>
<li><a href="#configuring_clion_project">Configuring CLion Project</a></li>
<li><a href="#implementation_notes">Implementation Notes</a></li>
</ul>

<a name="getting_started"/>

Getting Started
---------------

Start with Ubuntu 16.04 (either desktop or server distribution) or OSX. Windows is not being tested at this time.

Install RocksDB required libraries:

-	https://github.com/facebook/rocksdb/blob/master/INSTALL.md#supported-platforms

Get the sources:

```
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

Obviously the use of an IDE is a personal preference -- CLion is not required for fptreedb development (it's not free), but it's very easy to configure if you have a valid license.

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

The .gitignore for this project ignores CMakeLists.txt as well as the entire .idea directory, so no CLion-specific files should ever be committed.

<a name="implementation_notes"/>

Implementation Notes
--------------------
