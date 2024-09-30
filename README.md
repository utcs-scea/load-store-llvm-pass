# LLVM Globalize Pass Project

## Building the Pass

- Ensure you have LLVM and CMake installed on your system.
- Build O3 via `make`.
  - O3 binary will be `./a.out`.
- Optionally, build an O0 binary via `make o0`. The O0 binary will be `./a.u.out`. 

## Running Tests

- Run tests via `make test`.
  - Compiler output is generally parsed from console. 
  - If you are altering tests, remove the /dev/null redirect from tests/run_tests.sh.
