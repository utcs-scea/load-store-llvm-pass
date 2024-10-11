#include <stdio.h>

void use(int); 
int f1(); 
int f2(); 

void test(int i) { 
    int x; 
    if (i) {
        printf("f1 case!");
        x = f1(); 
    } else { 
        printf("f2 case!");
        x = f2(); 
    } 
    use(x);
}