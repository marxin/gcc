#include <stdio.h>

int foo(int a)
{
  void *l = &&error; 

  if(a == 4)
    goto *l;

  return 150;

  error:
    return a;
  failure:
    return a + 2;
}

int foo_wrong(int a)
{
  void *l = &&failure; 

  if(a == 4)
    goto *l;

  return 150;

  error:
    return a;
  failure:
    return a + 2;
}

int main(int argc, char **argv)
{
  printf("value: %d\n", foo(argc));

  return 0;
}
