CC = clang-18
OPT = opt

OPTFLAGS += --load-pass-plugin=../target/debug/libload_store_llvm_pass.dylib
OPTFLAGS += --load-pass-plugin=../build/LLVMGlobalizePass.so
OPTFLAGS += --passes=globalize-pass,load-store-pass

build_passes:
	cd .. && make build_passes

test:
	$(CC) -S -O3 $(testfile) -emit-llvm -o $(testfile).base.ll
	$(OPT) -S $(OPTFLAGS) $(testfile).base.ll -o $(testfile).passes_run.ll
	$(CC) -S -O3 -flto $(testfile).passes_run.ll -emit-llvm -o $(testfile).ll
	$(CC) -O3 -flto $(testfile).ll -o $(testfile).binary

clean:
	rm -f *.ll *.binary *.out