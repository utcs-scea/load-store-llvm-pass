#include <assert.h>
#include <stdio.h>
#include <string.h>

extern "C" {

int check_if_global(void* ptr) {
  // printf("   >> check_if_global() invoked\n");
  uintptr_t p = (uintptr_t) ptr;
  return (p >> 48) == 0xFFFF;
}

void* deglobalify(void* ptr) {
  // printf("   >> deglobalify() invoked\n");
  uintptr_t p = (uintptr_t) ptr;
  uintptr_t mask = ((uintptr_t)0xFFFF) << 48;
  return (void *) (p & ~mask);
}

void* globalify(void* ptr) {
  // printf("   >> globalify() invoked\n");
  uintptr_t p = (uintptr_t) ptr;
  uintptr_t mask = ((uintptr_t)0xFFFF) << 48;
  return (void *) (p | mask);
}

void __pando__replace_store_int64(uint64_t val, uint64_t* dst) {
  // printf("   >> __pando__replace_store_int64() invoked\n");
  assert(check_if_global(dst));
  *(uint64_t*) deglobalify(dst) = val;
}

void __pando__replace_store_int32(uint32_t val, uint32_t* dst) {
  // printf("   >> __pando__replace_store_int32() invoked\n");
  assert(check_if_global(dst));
  *(uint32_t*) deglobalify(dst) = val;
}

void __pando__replace_store_int16(uint16_t val, uint16_t* dst) {
  // printf("   >> __pando__replace_store_int16() invoked\n");
  assert(check_if_global(dst));
  *(uint16_t*) deglobalify(dst) = val;
}

void __pando__replace_store_int8(uint8_t val, uint8_t* dst) {
  // printf("   >> __pando__replace_store_int8() invoked\n");
  assert(check_if_global(dst));
  *(uint8_t*) deglobalify(dst) = val;
}

// void __pando__replace_store_int1(bool val, bool* dst) {
//   printf("   >> __pando__replace_store_int1() invoked\n");
//   assert(check_if_global(dst));
//   *(bool*) deglobalify(dst) = val;
// }

void __pando__replace_store_float64(double val, double* dst) {
  // printf("   >> __pando__replace_store_float64() invoked\n");
  assert(check_if_global(dst));
  *(double*) deglobalify(dst) = val;
}

void __pando__replace_store_float32(float val, float* dst) {
  // printf("   >> __pando__replace_store_float32() invoked\n");
  assert(check_if_global(dst));
  *(float*) deglobalify(dst) = val;
}

void __pando__replace_store_ptr(void* val, void** dst) {
  // printf("   >> __pando__replace_store_ptr() invoked\n");
  assert(check_if_global(dst));
  *(void**) deglobalify(dst) = val;
}

void __pando__replace_store_vector(void* val, void* dst, size_t element_size,
                                   size_t num_elements) {
  // printf("  >> __pando__replace_store_vector invoked\n");
  assert(check_if_global(dst));
  memcpy(deglobalify(dst), val, element_size * num_elements);
}

uint64_t __pando__replace_load_int64(uint64_t* src) {
  // printf("   >> __pando__replace_load_int64() invoked\n");
  assert(check_if_global(src));
  return *(uint64_t*) deglobalify(src);
}

uint32_t __pando__replace_load_int32(uint32_t* src) {
  // printf("   >> __pando__replace_load_int32() invoked\n");
  assert(check_if_global(src));
  return *(uint32_t*) deglobalify(src);
}

uint16_t __pando__replace_load_int16(uint16_t* src) {
  // printf("   >> __pando__replace_load_int16() invoked\n");
  assert(check_if_global(src));
  return *(uint16_t*) deglobalify(src);
}

uint8_t __pando__replace_load_int8(uint8_t* src) {
  // printf("   >> __pando__replace_load_int8() invoked\n");
  assert(check_if_global(src));
  return *(uint8_t*) deglobalify(src);
}

// bool __pando__replace_load_int1(bool* src) {
//   printf("   >> __pando__replace_load_int1() invoked\n");
//   assert(check_if_global(src));
//   return *(bool*) deglobalify(src);
// }

double __pando__replace_load_float64(double* src) {
  // printf("   >> __pando__replace_load_float64() invoked\n");
  assert(check_if_global(src));
  return *(double*) deglobalify(src);
}

float __pando__replace_load_float32(float* src) {
  // printf("   >> __pando__replace_load_float32() invoked\n");
  assert(check_if_global(src));
  return *(float*) deglobalify(src);
}

void* __pando__replace_load_ptr(void** src) {
  // printf("   >> __pando__replace_load_ptr() invoked\n");
  assert(check_if_global(src));
  // return globalify(*(uint64_t**) deglobalify(src));
  return *(uint64_t**) deglobalify(src);
}

void* __pando__replace_load_vector(void* src, size_t element_size, 
                                   size_t num_elements) {
  // printf("  >> __pando__replace_load_vector invoked\n");
  assert(check_if_global(src));
  // later, if not local, we will need to allocate memory 
  // and actually do the remote load.
  return deglobalify(src);
}

} // extern "C"
