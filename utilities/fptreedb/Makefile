include ../../make_config.mk

.PHONY: clean librocksdb

all: example test

example:
	$(CXX) $(CXXFLAGS) fptreedb.cc fptreedb_example.cc -o fptreedb_example ../../librocksdb.a \
	/usr/local/lib/libpmemobj.a /usr/local/lib/libpmem.a -I../../include \
	-O2 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)
	./fptreedb_example

test:
	$(CXX) $(CXXFLAGS) fptreedb.cc ../../third-party/gtest-1.7.0/fused-src/gtest/gtest-all.cc  \
	fptreedb_test.cc -o fptreedb_test ../../librocksdb.a /usr/local/lib/libpmemobj.a \
	/usr/local/lib/libpmem.a -I../../include -I../../third-party/gtest-1.7.0/fused-src \
	-O2 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)
	./fptreedb_test

clean:
	rm -rf fptreedb_example fptreedb_test
