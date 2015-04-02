#include"klee.h"
#include<assert.h>
#include<stdlib.h>

int v[100];

int main() {
  int x=1 , y=2;
  int  i = 0;

  klee_make_symbolic(&x, sizeof(x), "x");
  klee_make_symbolic(&y, sizeof(y), "y");

  if(0 != x*y) {
    i=  99;    
  } else {
    if(y == 0) {
      if(x == 3) {
        i = 1;
      } else {
        i = 4;
      }
    }
  }

  if(4 != x*y) {
    i=  99;    
  } else {
    if(y == 2) {
      if(x == 2) {
        i = -1;
      } else {
        i = 4;
      }
    }
  }

  return v[i] ;
}
