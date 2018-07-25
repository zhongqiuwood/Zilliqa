
all: zilliqa


zilliqa2:
	if [ -f build/bin/zilliqa ]; then
		rm build/bin/zilliqa
	fi
	mkdir -p build
	cd build && \
	cmake -D \
	STATIC=ON -D \
	BUILD_SHARED_LIBS=OFF -D CMAKE_BUILD_TYPE=Debug .. && make -j 6 VERBOSE=1

zilliqa:
	cd build && make -j 6 VERBOSE=1

# dynamically link all system libraries and all dependence libraries in monero/src dir
dynamic_on:
	mkdir -p build/dynamic_on
	cd build/dynamic_on && cmake -D \
	BUILD_TESTS=OFF -D \
	STATIC=OFF -D \
	BUILD_SHARED_LIBS=ON -D \
	CMAKE_BUILD_TYPE=Debug ../.. && $(MAKE)

# statically link all system libraries and all dependence libraries in monero/src dir
static_on:
	mkdir -p build/static_on
	cd build/static_on && cmake -D \
	BUILD_TESTS=OFF -D \
	STATIC=ON -D \
	BUILD_SHARED_LIBS=OFF -D \
	CMAKE_BUILD_TYPE=Debug ../.. && $(MAKE)

