#include <stdio.h>

int global_array[5] = {1, 2, 3, 4, 5};
int *global_ptr = global_array;

void modify_array() {
  for (int i = 0; i < 5; i++) {
    global_array[i] *= 2;
  }
}

int main() {
  printf("Initial array: ");
  for (int i = 0; i < 5; i++) {
    printf("%d ", global_ptr[i]);
  }
  printf("\n");

  modify_array();

  printf("Modified array: ");
  for (int i = 0; i < 5; i++) {
    printf("%d ", global_array[i]);
  }
  printf("\n");

  return 0;
}
