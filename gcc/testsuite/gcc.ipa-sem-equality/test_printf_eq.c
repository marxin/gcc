#include <stdio.h>

int foo()
{
  printf ("Hello world.\n");
  return 0;
}

int bar()
{
  printf ("Hello world.\n");
  return 0;
}

int main()
{
  return foo() + bar();
}
