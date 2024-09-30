CC = clang-18
OPT = opt

OPTFLAGS += --load-pass-plugin=../target/debug/libload_store_llvm_pass.dylib
OPTFLAGS += --load-pass-plugin=../build/LLVMGlobalizePass.so
OPTFLAGS += --passes=globalize-pass,load-store-pass

test: clean
	./run_tests.sh

test_o0: clean
	./run_tests_o0.sh

build_passes:
	cd .. && make build_passes

pando_functions.ll: pando_functions.cc
	$(CC) -S -O3 $< -emit-llvm -o $@

run_test: pando_functions.ll
	$(CC) -S -O3 $(testfile) -emit-llvm -o $(testfile).base.ll
	llvm-link -S $(testfile).base.ll pando_functions.ll -o $(testfile).linked.ll
	$(OPT) -S $(OPTFLAGS) $(testfile).linked.ll -o $(testfile).passes_run.ll
	$(CC) -S -O3 -flto $(testfile).passes_run.ll -emit-llvm -o $(testfile).final.ll
	$(CC) -O3 -flto $(testfile).final.ll -o $(testfile).binary

run_test_o0: pando_functions.ll
	$(CC) -S -O0 $(testfile) -emit-llvm -o $(testfile).base.ll
	llvm-link -S $(testfile).base.ll pando_functions.ll -o $(testfile).linked.ll
	$(OPT) -S $(OPTFLAGS) $(testfile).linked.ll -o $(testfile).passes_run.ll
	$(CC) -S -O0 -flto $(testfile).passes_run.ll -emit-llvm -o $(testfile).final.ll
	$(CC) -O0 -flto $(testfile).final.ll -o $(testfile).binary

clean:
	rm -f *.ll *.binary *.out
