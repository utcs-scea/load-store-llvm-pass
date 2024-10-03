#include <stdio.h>
#include <stdint.h>

uint64_t hello = 5;

int main() {
  uint64_t* bravo = &hello;
  printf("hello is == %llu.\n", *bravo);
}

