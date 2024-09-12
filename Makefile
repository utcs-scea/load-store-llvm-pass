CC = clang-18
# CCFLAGS =

OPT = opt
OPTFLAGS = --load-pass-plugin=./target/debug/libload_store_llvm_pass.dylib

OPTFLAGS += --passes=globalify-consts-pass,load-store-pass
# OPTFLAGS += --passes=load-store-pass

OPTFLAGS += -S

all: clean a.out main.u.ll

a.out: a.out.ll
	$(CC) -O3 $? -flto -o $@

a.out.ll: load_patch.ll
	$(CC) -S -O3 $? -flto -o $@ -emit-llvm 

load_patch.ll: main.ll ./src/lib.rs
	cargo build
	$(OPT) $(OPTFLAGS) $< -o $@

load_patch.u.ll: main.u.ll ./src/lib.rs
	cargo build
	$(OPT) $(OPTFLAGS) $< -o $@

main.ll: ./src/main.c
	$(CC) -S -O3 $? -o $@ -emit-llvm

main.u.ll: ./src/main.c
	$(CC) -S -O0 $? -o $@ -emit-llvm

clean:
	rm -f *.ll *.out
