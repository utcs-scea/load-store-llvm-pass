#include <stdio.h>

int global_a = 5;
float global_b = 3.14;
char global_c = 'X';

void print_globals() {
  printf("global_a: %d, global_b: %.2f, global_c: %c\n", global_a, global_b, global_c);
}

int main() {
  print_globals();
  global_a *= 2;
  global_b += 1.0;
  global_c = 'Y';
  print_globals();
  return 0;
}
