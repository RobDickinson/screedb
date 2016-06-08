# fptreedb
Fingerprinted Persistent Trees for RocksDB

## Getting Started

Start with Ubuntu 16.04 (either desktop or server distribution)

Install RocksDB required libraries: 

* https://github.com/facebook/rocksdb/blob/master/INSTALL.md#supported-platforms

Get the sources:

```
git clone https://github.com/RobDickinson/fptreedb.git
```

## Building and Running Tests

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

## Configuring CLion Project

Delete original CMakeLists.txt file (it's for Windows build).

Add CMakeLists.txt to .gitignore, since CLion will overwrite this at will.

For Toolchain configuration, use bundled CMake.

Use wizard to create project:

* From Welcome screen, select "Import Project From Sources"
* select FPTREEDB_HOME directory
* select "Overwrite CMakeLists.txt"
* deselect all directories, then select db

Add these lines to CMakeLists.txt:

```
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/third-party/gtest-1.7.0/fused-src)
```

In Project View, right-click on db|examples|include|memtable|table|util|utilities directories and select Mark Directory As | Project Sources and Headers. Close CLion, and verify that .idea/fptreedb.iml now looks like this:

```
<content url="file://$MODULE_DIR$">
  <sourceFolder url="file://$MODULE_DIR$/CMakeLists.txt" isTestSource="false" />
  <sourceFolder url="file://$MODULE_DIR$/src.mk" isTestSource="false" />
  <sourceFolder url="file://$MODULE_DIR$/db" isTestSource="false" />
  <sourceFolder url="file://$MODULE_DIR$/examples" isTestSource="false" />
  <sourceFolder url="file://$MODULE_DIR$/include" isTestSource="false" />
  <sourceFolder url="file://$MODULE_DIR$/memtable" isTestSource="false" />
  <sourceFolder url="file://$MODULE_DIR$/table" isTestSource="false" />
  <sourceFolder url="file://$MODULE_DIR$/util" isTestSource="false" />
  <sourceFolder url="file://$MODULE_DIR$/utilities" isTestSource="false" />
</content>
```
