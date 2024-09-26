CC = clang-18
OPT = opt

OPTFLAGS += --load-pass-plugin=./target/debug/libload_store_llvm_pass.dylib
OPTFLAGS += --load-pass-plugin=./build/LLVMGlobalizePass.so
OPTFLAGS += --passes=globalize-pass,load-store-pass
OPTFLAGS += -S

all: clean build_passes a.out

o0: clean build_passes a.u.out


# build rust and c++ pass files
build_passes:
	cd build && cmake .. && make
	cargo build


# build O3
a.out: a.out.ll
	$(CC) -O3 $? -flto -o $@

a.out.ll: testbench_passes_run.ll
	$(CC) -S -O3 $? -flto -o $@ -emit-llvm 

testbench_passes_run.ll: testbench.ll ./src/lib.rs
	$(OPT) $(OPTFLAGS) $< -o $@

testbench.ll: testbench.c
	$(CC) -S -O3 $? -o $@ -emit-llvm


# build O0
a.u.out: a.u.out.ll
	$(CC) -O0 $? -flto -o $@

a.u.out.ll: testbench_passes_run.u.ll
	$(CC) -S -O0 $? -flto -o $@ -emit-llvm 

testbench_passes_run.u.ll: testbench.u.ll ./src/lib.rs
	$(OPT) $(OPTFLAGS) $< -o $@
	
testbench.u.ll: testbench.c
	$(CC) -S -O0 $? -o $@ -emit-llvm


# clean
clean:
	rm -rf *.ll *.out
	cd build && make clean
