#include"klee.h"
#include <stdlib.h>

int foo(int x) {
  int *ptr;
  int array[5] ;

  ptr = array + x;
   
  int temp =  *ptr;
  return x;
}


int main() {
  int x;

  klee_make_symbolic(&x, sizeof(x), "x");
  klee_assume(x >= 0);
  klee_assume( x <= 4);

  return foo(x);
}

