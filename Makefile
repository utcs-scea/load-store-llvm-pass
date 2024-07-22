
a.out: a.out.ll
	clang-18 -O3 $? -o $@ -flto
a.out.ll: load_patch.ll
	clang-18 -O3 $? -o $@ -S -emit-llvm -flto

main.ll: main.c
	clang-18 -O3 $? -S -emit-llvm -o $@

load_patch.ll: main.ll ./src/lib.rs
	cargo build
	opt-18 --load-pass-plugin=./target/debug/libload_store_llvm_pass.so --passes=load-store-pass $< -S -o $@

load_patch.u.ll: main.u.ll ./src/lib.rs
	cargo build
	opt-18 --load-pass-plugin=load-store-llvm-pass/target/debug/libload_store_llvm_pass.so --passes=load-store-pass $< -S -o $@

main.u.ll: main.c
	clang-18 -O0 $? -S -emit-llvm -o $@

clean:
	rm -f *.ll *.out
