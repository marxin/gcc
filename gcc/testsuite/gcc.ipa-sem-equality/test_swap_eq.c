#include <stdio.h>

int fce1(int a, int b)
{
   int swap;

   if(a < b)
   {
      swap = a;
      a = b;
      b = swap;
   }

   return a / b;
}

int fce2(int x, int y)
{
   int tmp;

   if(x < y)
   {
      tmp = x;
      x = y;
      y = tmp;
   }

   return x / y;
}


int main(int argc, char **argv)
{
   printf("fce1: %d\n", fce1(argc, argc + 2));
   printf("fce2: %d\n", fce2(argc, 2 * argc));
}
