CC = clang++
OPT = opt

CPP_PASSFILE += --load-pass-plugin=../build/LLVMGlobalizePass.so
RUSTPASSFILE += --load-pass-plugin=../target/debug/libload_store_llvm_pass.dylib

GLOBALIZEPASS += --passes=globalize-pass
LOADSTOREPASS += --passes=load-store-pass
LIBFNCALLSPASS += --passes=wrap-lib-fn-calls-pass
GLOBALPTRSPASS += --passes=remotify-global-ptr-vars-pass

test: clean
	./run_tests.sh

test_o0: clean
	./run_tests_o0.sh

build_passes:
	cd .. && make build_passes

run_test:
	$(CC) -S -O3 $(testfile) -emit-llvm -o $(testfile).base.ll
	llvm-link -S $(testfile).base.ll ../pando_functions.ll -o $(testfile).linked.ll
	$(OPT) -S $(CPP_PASSFILE) $(GLOBALIZEPASS) $(testfile).linked.ll -o $(testfile).globalized.ll
	$(OPT) -S $(RUSTPASSFILE) $(LOADSTOREPASS) $(testfile).globalized.ll -o $(testfile).load_store.ll
	$(OPT) -S $(CPP_PASSFILE) $(LIBFNCALLSPASS) $(testfile).load_store.ll -o $(testfile).lib_params.ll
	$(OPT) -S $(CPP_PASSFILE) $(GLOBALPTRSPASS) $(testfile).lib_params.ll -o $(testfile).globalptrs.ll
	$(CC) -S -O3 -flto $(testfile).globalptrs.ll -emit-llvm -o $(testfile).final.ll
	$(CC) -O3 -flto $(testfile).final.ll -o $(testfile).binary

run_test_o0:
	$(CC) -S -O0 $(testfile) -emit-llvm -o $(testfile).base.ll
	llvm-link -S $(testfile).base.ll ../pando_functions.ll -o $(testfile).linked.ll
	$(OPT) -S $(CPP_PASSFILE) $(GLOBALIZEPASS) $(testfile).linked.ll -o $(testfile).globalized.ll
	$(OPT) -S $(RUSTPASSFILE) $(LOADSTOREPASS) $(testfile).globalized.ll -o $(testfile).load_store.ll
	$(OPT) -S $(CPP_PASSFILE) $(LIBFNCALLSPASS) $(testfile).load_store.ll -o $(testfile).lib_params.ll
	$(OPT) -S $(CPP_PASSFILE) $(GLOBALPTRSPASS) $(testfile).lib_params.ll -o $(testfile).globalptrs.ll
	$(CC) -S -O0 -flto $(testfile).globalptrs.ll -emit-llvm -o $(testfile).final.ll
	$(CC) -O0 -flto $(testfile).final.ll -o $(testfile).binary

clean:
	rm -f *.ll *.binary *.out
