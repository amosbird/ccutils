##
# ccutils
#
# @file
# @version 0.1

PREFIX=/usr/local
CPPFLAGS = -MMD -MP
CXXFLAGS ?= -std=c++17
CXX_DBG_FLAGS = $(CXXFLAGS) -g
CXX ?= g++
LD = $(CXX)
LDFLAGS := $(CXXFLAGS) $(LDFLAGS)

install:
	cp -r src $(PREFIX)/include/ccutils

all.o: test/all.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Isrc -c $< -o $@

t: all.o
	$(LD) $< -o $@ $(LDFLAGS) -lpthread

# end
