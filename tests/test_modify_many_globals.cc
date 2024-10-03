#include <stdio.h>

int global_counter = 0;
float global_factor = 1.5;
char global_flag = 'N';

void increment_counter() {
  global_counter++;
}

void apply_factor() {
  global_counter *= global_factor;
}

void toggle_flag() {
  global_flag = (global_flag == 'Y') ? 'N' : 'Y';
}

void print_globals() {
  printf("Counter: %d, Factor: %.2f, Flag: %c\n", global_counter, global_factor, global_flag);
}

int main() {
  printf("Initial state:\n");
  print_globals();

  for (int i = 0; i < 3; i++) {
    increment_counter();
  }
  printf("After incrementing:\n");
  print_globals();

  apply_factor();
  printf("After applying factor:\n");
  print_globals();

  toggle_flag();
  global_factor += 0.5;
  printf("After toggling flag and increasing factor:\n");
  print_globals();

  return 0;
}
