#include <assert.h>
#include <stdint.h>
#include <stdio.h>

uint64_t hello = 5;

int check_if_global(void* ptr) {
  uintptr_t p = (uintptr_t) ptr;
  return (p >> 48) == 0xFFFF;
}

void* deglobalify(void* ptr) {
  uintptr_t p = (uintptr_t) ptr;
  uintptr_t mask = ((uintptr_t)0xFFFF) << 48;
  return (void *) (p & ~mask);
}

void* globalify(void* ptr) {
  uintptr_t p = (uintptr_t) ptr;
  uintptr_t mask = ((uintptr_t)0xFFFF) << 48;
  return (void *) (p | mask);
}

void __pando__replace_store64(uint64_t val, uint64_t* dst) {
  assert(check_if_global(dst));
  *(uint64_t*) deglobalify(dst) = val;
}

void __pando__replace_store32(uint32_t val, uint64_t* dst) {
  assert(check_if_global(dst));
  *(uint64_t*) deglobalify(dst) = val;
}

void __pando__replace_storeptr(void* val, void** dst) {
  assert(check_if_global(dst));
  *(void**) deglobalify(dst) = val;
}

uint64_t __pando__replace_load64(uint64_t* src) {
  return *(uint64_t*) deglobalify(src);
}

uint32_t __pando__replace_load32(uint32_t* src) {
  return *(uint32_t*) deglobalify(src);
}

void* __pando__replace_loadptr(void** src) {
  return *(uint64_t**) deglobalify(src);
}

int main() {
  uint64_t* bravo = &hello;
  assert(check_if_global(bravo));
  printf("hello is == %llu.\n", *bravo);
}
