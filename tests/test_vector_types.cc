#include <stdio.h>

int global_array[5] = {1, 2, 3, 4, 5};
int *global_ptr = global_array;

void modify_array() {
  for (int i = 0; i < 5; i++) {
    global_array[i] *= 2;
  }
}

int main() {
  printf("Initial array: \n");
  for (int i = 0; i < 5; i++) {
    printf("%d \n", global_ptr[i]);
  }

  modify_array();

  printf("Modified array: \n");
  for (int i = 0; i < 5; i++) {
    printf("%d \n", global_array[i]);
  }

  return 0;
}
