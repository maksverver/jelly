CXXFLAGS=-std=c++20 -Wall -Wextra -Wno-sign-compare
OPT_FLAGS=-O3
DBG_FLAGS=-Og -g -DNDEBUG

SRCS=solve.cc
BINS=solve.dbg solve.opt

all: $(BINS)

solve.dbg: $(SRC)
	$(CXX) $(CXXFLAGS) $(DBG_FLAGS) -o $@ $(SRCS)

solve.opt: $(SRC)
	$(CXX) $(CXXFLAGS) $(OPT_FLAGS) -o $@ $(SRCS)

clean:
	rm -f $(BINS)
