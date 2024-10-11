CXX = clang++
OPT = opt

CXXFLAGS = -std=c++11 -Wall -Wextra -O3

CPP_PASSFILE += --load-pass-plugin=../../build/LLVMGlobalizePass.so
RUSTPASSFILE += --load-pass-plugin=../../target/debug/libload_store_llvm_pass.dylib

GLOBALIZEPASS += --passes=globalize-pass
LOADSTOREPASS += --passes=load-store-pass
LIBPARAMSPASS += --passes=unwrap-lib-fn-call-params-pass
UNNAMEDGLOBALPTRSPASS += --passes=globalize-global-unnamed-pointers-pass

all: bfs.out bfs_pass.out

bfs.out: bfs.cc
	$(CXX) -flto $(CXXFLAGS) $^ -o $@ 

bfs_pass.out: bfs.cc
	$(CXX) -S $(CXXFLAGS) $< -emit-llvm -o $@.base.ll
	llvm-link -S $@.base.ll ../../tests/pando_functions.ll -o $@.linked.ll
	$(OPT) -S $(CPP_PASSFILE) $(GLOBALIZEPASS) $@.linked.ll -o $@.globalized.ll
	$(OPT) -S $(RUSTPASSFILE) $(LOADSTOREPASS) $@.globalized.ll -o $@.load_store.ll
	$(OPT) -S $(CPP_PASSFILE) $(LIBPARAMSPASS) $@.load_store.ll -o $@.lib_params.ll
	$(OPT) -S $(CPP_PASSFILE) $(UNNAMEDGLOBALPTRSPASS) $@.lib_params.ll -o $@.unnamedglobalptrs.ll
	$(CXX) -S -flto $(CXXFLAGS) $@.unnamedglobalptrs.ll -emit-llvm -o $@.final.ll
	$(CXX) -flto $(CXXFLAGS) $@.final.ll -o $@

clean:
	rm -rf bfs.out bfs.dSYM bfs_pass.*

.PHONY: all clean

debug: CXXFLAGS += -g -DDEBUG
debug: all

help:
	@echo "Available targets:"
	@echo "  all    : Build the executable (default)"
	@echo "  clean  : Remove object files and the executable"
	@echo "  debug  : Build with debugging symbols"
	@echo "  run    : Build and run the executable"
	@echo "  help   : Display this help message"