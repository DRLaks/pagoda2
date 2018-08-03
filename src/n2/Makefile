CC=/usr/local/clang4/bin/clang
CXX=/usr/local/clang4/bin/clang++
CXX11=/usr/local/clang4/bin/clang++
CXX14=/usr/local/clang4/bin/clang++
CXX17=/usr/local/clang4/bin/clang++
CXX1X=/usr/local/clang4/bin/clang++
LDFLAGS=-L/usr/local/clang4/lib
CXX ?= g++
PKG_CXXFLAGS = -O3 -march=native -std=c++11 -pthread -fPIC -fopenmp
PKG_CXXFLAGS += -I"./include"
LDFLAGS += -lpthread

LIB = ../libn2.a
lib: $(LIB)

$(LIB): base.o hnsw.o hnsw_node.o distance.o heuristic.o mmap.o
	ar rvs $@ $?

#libn2.so: base.o hnsw.o hnsw_node.o distance.o heuristic.o mmap.o
#	$(CXX) $(CXXFLAGS) -shared -o $@ $(LDFLAGS) $?

clean:
	@-rm -f *.o $(LIB) 


