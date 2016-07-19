include ../../make_config.mk

.PHONY: clean librocksdb

all: example test stress

example:
	$(CXX) $(CXXFLAGS) screedb.cc screedb_example.cc -o screedb_example ../../librocksdb.a \
	/usr/local/lib/libpmemobj.a /usr/local/lib/libpmem.a -I../../include \
	-O2 -std=c++11 -ldl $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)
	./screedb_example

test:
	$(CXX) $(CXXFLAGS) screedb.cc ../../third-party/gtest-1.7.0/fused-src/gtest/gtest-all.cc  \
	screedb_test.cc -o screedb_test ../../librocksdb.a /usr/local/lib/libpmemobj.a \
	/usr/local/lib/libpmem.a -I../../include -I../../third-party/gtest-1.7.0/fused-src \
	-O2 -std=c++11 -ldl $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)
	./screedb_test

stress:
	$(CXX) $(CXXFLAGS) screedb.cc screedb_stress.cc -o screedb_stress ../../librocksdb.a \
	/usr/local/lib/libpmemobj.a /usr/local/lib/libpmem.a -I../../include \
	-O2 -std=c++11 -ldl $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)
	rm -rf /dev/shm/screedb_stress
	./screedb_stress

clean:
	rm -rf screedb_example screedb_stress screedb_test
	rm -rf /dev/shm/screedb_example /dev/shm/screedb_stress /dev/shm/screedb_test
