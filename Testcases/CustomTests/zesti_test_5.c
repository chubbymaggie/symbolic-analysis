#include "klee.h"
#include "assert.h"

int v [100];
void func(int x)
{

  if(x == 0 || x == 99) {
    v[x] = 10;
  } else {
    v[x] = 0 ;
  }
}

int main()
{

  int x = 99;  //  detect no sens inst; but the concreat path does not
                //  initialte any error. with use-symbex>1, additional paths
                //  are not explored.
  klee_make_symbolic(&x, sizeof(x), "x");

  func(x);

}
