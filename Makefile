CC = clang++

all: build_passes pando_functions.ll

# build rust and c++ pass files
build_passes:
	cd build && cmake .. && make
	cargo build

# build pando functions file
pando_functions.ll: pando_functions.cc
	$(CC) -S -O3 $< -emit-llvm -o $@

# run tests
test: build_passes test_o0 test_o3

test_o3:
	cd tests && make test

test_o0:
	cd tests && make test_o0

clean:
	rm -f pando_functions.ll
