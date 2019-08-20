
_BITS=$(shell java -version 2>&1 | sed -n -E "s/[a-zA-Z\(\) ]+ ([0-9]+)-Bit.*/\1/p")
_ARCH=$(shell uname | tr A-Z a-z)
HEADERS=$(wildcard include/*.h)
LIBSOURCE=$(wildcard src/lib/*.cpp)
TARGET=FastEventServer_$(_ARCH)_$(_BITS)bit
PROFILE=profile_direct_$(_ARCH)_$(_BITS)bit
CCOPTS=-Iinclude -Ilibks/include -Wall -O3 
LDOPTS=-Llibks -lks -lpthread

.PHONY: all libks
all: libks 
	$(MAKE) $(TARGET)
	$(MAKE) $(PROFILE)

$(TARGET): src/main.cpp $(LIBSOURCE) $(HEADERS) libks/libks.a
	g++ $(CCOPTS) -o $@ $< $(LIBSOURCE) $(LDOPTS)

libks:
	$(MAKE) -C libks

$(PROFILE): src/profile_direct.cpp $(LIBSOURCE) $(HEADERS) libks/libks.a
	g++ $(CCOPTS) -o $@ $< $(LIBSOURCE) $(LDOPTS)

