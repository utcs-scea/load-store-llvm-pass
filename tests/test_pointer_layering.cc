#include <stdio.h>

int global_a = 10;
int global_b = 20;
int *global_ptr_a = &global_a;
int *global_ptr_b = &global_b;

void swap_pointers() {
  int *temp = global_ptr_a;
  global_ptr_a = global_ptr_b;
  global_ptr_b = temp;
}

void modify_through_pointers() {
  *global_ptr_a += 5;
  *global_ptr_b *= 2;
}

int main() {
  printf("Initial values: global_a = %d, global_b = %d\n", global_a, global_b);
  printf("Initial pointer values: *global_ptr_a = %d, *global_ptr_b = %d\n", *global_ptr_a, *global_ptr_b);

  modify_through_pointers();
  printf("After modification: global_a = %d, global_b = %d\n", global_a, global_b);

  swap_pointers();
  printf("After swapping pointers: *global_ptr_a = %d, *global_ptr_b = %d\n", *global_ptr_a, *global_ptr_b);

  modify_through_pointers();
  printf("Final values: global_a = %d, global_b = %d\n", global_a, global_b);

  return 0;
}
