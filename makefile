
_BITS=$(shell java -version 2>&1 | sed -n -E "s/[a-zA-Z\(\) ]+ ([0-9]+)-Bit.*/\1/p")
_ARCH=$(shell uname | tr A-Z a-z)
HEADERS=$(wildcard include/*.h)
LIBSOURCE=$(wildcard src/lib/*.cpp)
TARGET=FastEventServer_$(_ARCH)_$(_BITS)bit
OPT=-Iinclude -Ilibks/include -Llibks -lks -Wall -O3

$(TARGET): src/main.cpp $(LIBSOURCE) $(HEADERS)
	g++ $(OPT) $< $(LIBSOURCE) -o $@

profile_direct: %: src/%.cpp $(LIBSOURCE) $(HEADERS)
	g++ $(OPT) $< $(LIBSOURCE) -o $@

