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

void __pando__replace_store_int64(uint64_t val, uint64_t* dst) {
  assert(check_if_global(dst));
  *(uint64_t*) deglobalify(dst) = val;
}

void __pando__replace_store_int32(uint32_t val, uint32_t* dst) {
  assert(check_if_global(dst));
  *(uint32_t*) deglobalify(dst) = val;
}

void __pando__replace_store_int8(uint8_t val, uint8_t* dst) {
  assert(check_if_global(dst));
  *(uint8_t*) deglobalify(dst) = val;
}

void __pando__replace_store_float32(float val, float* dst) {
  assert(check_if_global(dst));
  *(float*) deglobalify(dst) = val;
}

void __pando__replace_store_ptr(void* val, void** dst) {
  assert(check_if_global(dst));
  *(void**) deglobalify(dst) = val;
}

uint64_t __pando__replace_load_int64(uint64_t* src) {
  return *(uint64_t*) deglobalify(src);
}

uint32_t __pando__replace_load_int32(uint32_t* src) {
  return *(uint32_t*) deglobalify(src);
}

uint8_t __pando__replace_load_int8(uint8_t* src) {
  return *(uint8_t*) deglobalify(src);
}

float __pando__replace_load_float32(float* src) {
  return *(float*) deglobalify(src);
}

void* __pando__replace_load_ptr(void** src) {
  return *(uint64_t**) deglobalify(src);
}

int main() {
  uint64_t* bravo = &hello;
  assert(check_if_global(bravo));
  printf("hello is == %llu.\n", *bravo);
}
