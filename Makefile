CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude -Ithird_party

LIB_SOURCES := $(wildcard src/*.cpp)
LIB_OBJECTS := $(LIB_SOURCES:.cpp=.o)
STATIC_LIB := build/libwater_quality253.a

.PHONY: all test clean example quality-example
all: $(STATIC_LIB) build/water_quality253_selftest build/water_quality253 build/water_quality253_quality

build:
	mkdir -p build

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(STATIC_LIB): $(LIB_OBJECTS) | build
	ar rcs $@ $(LIB_OBJECTS)

build/water_quality253_selftest: tests/selftest.cpp $(LIB_OBJECTS) | build
	$(CXX) $(CXXFLAGS) $< $(LIB_OBJECTS) -o $@

build/water_quality253: examples/solve_stdin.cpp $(LIB_OBJECTS) | build
	$(CXX) $(CXXFLAGS) $< $(LIB_OBJECTS) -o $@

build/water_quality253_quality: examples/quality_stdin.cpp $(LIB_OBJECTS) | build
	$(CXX) $(CXXFLAGS) $< $(LIB_OBJECTS) -o $@

test: build/water_quality253_selftest
	./build/water_quality253_selftest

example: build/water_quality253
	./build/water_quality253 < examples/example.json

quality-example: build/water_quality253_quality
	./build/water_quality253_quality < examples/quality_example.json

clean:
	rm -f src/*.o
	rm -rf build
