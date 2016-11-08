include ../../make_config.mk

.PHONY: clean librocksdb

all: example test

example:
	$(CXX) $(CXXFLAGS) screedb.cc screedb_example.cc -o screedb_example ../../librocksdb.a \
	/usr/local/lib/libpmemobj.a /usr/local/lib/libpmem.a -I../../include \
	-O2 -std=c++11 -ldl $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)
	rm -rf /dev/shm/screedb
	PMEM_IS_PMEM_FORCE=1 ./screedb_example

test:
	$(CXX) $(CXXFLAGS) screedb.cc ../../third-party/gtest-1.7.0/fused-src/gtest/gtest-all.cc  \
	screedb_test.cc -o screedb_test ../../librocksdb.a /usr/local/lib/libpmemobj.a \
	/usr/local/lib/libpmem.a -I../../include -I../../third-party/gtest-1.7.0/fused-src \
	-O2 -std=c++11 -ldl $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)
	rm -rf /dev/shm/screedb
	PMEM_IS_PMEM_FORCE=1 ./screedb_test

stress_rocks:
	$(CXX) $(CXXFLAGS) screedb.cc screedb_stress_rocks.cc -o screedb_stress_rocks \
	../../librocksdb.a /usr/local/lib/libpmemobj.a /usr/local/lib/libpmem.a -I../../include \
	-DNDEBUG -O2 -std=c++11 -ldl $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)
	rm -rf /dev/shm/screedb
	PMEM_IS_PMEM_FORCE=1 ./screedb_stress_rocks

stress_tree:
	$(CXX) $(CXXFLAGS) screedb.cc screedb_stress_tree.cc -o screedb_stress_tree \
	../../librocksdb.a /usr/local/lib/libpmemobj.a /usr/local/lib/libpmem.a -I../../include \
	-DNDEBUG -O2 -std=c++11 -ldl $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)
	rm -rf /dev/shm/screedb
	PMEM_IS_PMEM_FORCE=1 ./screedb_stress_tree

clean:
	rm -rf /dev/shm/screedb
	rm -rf screedb_example screedb_stress_rocks screedb_stress_tree screedb_test
