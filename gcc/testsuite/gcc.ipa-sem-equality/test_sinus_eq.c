#include <math.h>

float foo()
{
  return sin(12.4f);
}

float bar()
{
  return sin(12.4f);
}

int main()
{
  foo();
  bar();

  return 0;
}
