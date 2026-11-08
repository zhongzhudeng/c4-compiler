BUILD_DIR ?= build/debug
BUILD_TYPE ?= Debug
CMAKE ?= cmake
CTEST ?= ctest

.PHONY: all configure test debug release clean

all: configure
	${CMAKE} --build ${BUILD_DIR}

configure:
	${CMAKE} -S . -B ${BUILD_DIR} -G Ninja -DCMAKE_BUILD_TYPE=${BUILD_TYPE}

test: all
	${CTEST} --test-dir ${BUILD_DIR} --output-on-failure

debug: all

release:
	${MAKE} BUILD_DIR=build/release BUILD_TYPE=Release all

clean:
	${CMAKE} -E rm -rf build
