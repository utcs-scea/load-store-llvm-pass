# LLVM Globalize Pass Project

## Building the Pass

- Ensure you have Rust, LLVM, and CMake installed on your system.
- Build the passes via `make` (or `make build_passes`). 

## Running Tests

- Run O3 tests via `make test`. Run O0 tests via `make test_o0`.

## PANDO Function Interface

- PANDO wrapper functions are currently in pando_functions.cc. 
- Note that the load_ptr function is idempotent.
  - e.g., if you invoke `__pando__replace_load_ptr()`, the returned pointer will always be a remote pointer.