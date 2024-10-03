# build rust and c++ pass files
build_passes:
	cd build && cmake .. && make
	cargo build

# run tests
test: build_passes
	cd tests && make test

test_o0: build_passes
	cd tests && make test_o0
