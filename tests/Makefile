CC = clang-18
OPT = opt

CPP_PASSFILE += --load-pass-plugin=../build/LLVMGlobalizePass.so
RUSTPASSFILE += --load-pass-plugin=../target/debug/libload_store_llvm_pass.dylib

GLOBALIZEPASS += --passes=globalize-pass
LOADSTOREPASS += --passes=load-store-pass
LIBPARAMSPASS += --passes=unwrap-lib-fn-call-params-pass
UNNAMEDGLOBALPTRSPASS += --passes=globalize-global-unnamed-pointers-pass

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
	$(OPT) -S $(CPP_PASSFILE) $(GLOBALIZEPASS) $(testfile).linked.ll -o $(testfile).globalized.ll
	$(OPT) -S $(RUSTPASSFILE) $(LOADSTOREPASS) $(testfile).globalized.ll -o $(testfile).load_store.ll
	$(OPT) -S $(CPP_PASSFILE) $(LIBPARAMSPASS) $(testfile).load_store.ll -o $(testfile).lib_params.ll
	$(OPT) -S $(CPP_PASSFILE) $(UNNAMEDGLOBALPTRSPASS) $(testfile).lib_params.ll -o $(testfile).unnamedglobalptrs.ll
	$(CC) -S -O3 -flto $(testfile).unnamedglobalptrs.ll -emit-llvm -o $(testfile).final.ll
	$(CC) -O3 -flto $(testfile).final.ll -o $(testfile).binary

run_test_o0: pando_functions.ll
	$(CC) -S -O0 $(testfile) -emit-llvm -o $(testfile).base.ll
	llvm-link -S $(testfile).base.ll pando_functions.ll -o $(testfile).linked.ll
	$(OPT) -S $(CPP_PASSFILE) $(GLOBALIZEPASS) $(testfile).linked.ll -o $(testfile).globalized.ll
	$(OPT) -S $(RUSTPASSFILE) $(LOADSTOREPASS) $(testfile).globalized.ll -o $(testfile).load_store.ll
	$(OPT) -S $(CPP_PASSFILE) $(LIBPARAMSPASS) $(testfile).load_store.ll -o $(testfile).lib_params.ll
	$(OPT) -S $(CPP_PASSFILE) $(UNNAMEDGLOBALPTRSPASS) $(testfile).lib_params.ll -o $(testfile).unnamedglobalptrs.ll
	$(CC) -S -O0 -flto $(testfile).unnamedglobalptrs.ll -emit-llvm -o $(testfile).final.ll
	$(CC) -O0 -flto $(testfile).final.ll -o $(testfile).binary

clean:
	rm -f *.ll *.binary *.out
