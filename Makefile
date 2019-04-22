.PHONY: install run_test clean

CXXFLAGS := -Wall -Wextra -Wpedantic -Wno-invalid-offsetof -std=c++17
CPPFLAGS := -I"$(PWD)/include" $(CPPFLAGS)
INSTALL_PREFIX ?= /usr/local

test_objs = $(shell find test/ | grep '\.cpp$$' | sed 's@\.cpp$$@.o@')

run_test: tests
	./tests

tests:  $(test_objs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -Wl,-start-group $(test_objs) -Wl,-end-group -o $@

${test_objs}: CPPFLAGS := -include test/abort-throw.hpp -I"${PWD}/vendor/catch2/single_include" $(CPPFLAGS)
$(test_objs): vendor/catch2/README.md

vendor/catch2/README.md:
	git submodule init vendor/catch2
	git submodule update vendor/catch2

install:
	echo cp -Rv include/* ${INSTALL_PREFIX}/include/

clean:
	rm -vf tests $(test_objs)
