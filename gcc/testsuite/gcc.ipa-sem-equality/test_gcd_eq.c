#include <stdlib.h>
#include <stdio.h>

int gcd(int x, int y) __attribute__ ((pure));

int gcd(int x, int y)
{
  int swap;

  if(x <= 0 || y <= 0)
    return 0;

  if(x < y)
  {
    swap = x;
    x = y;
    y = swap;
  }

  while(x != y)
  {
    x = x - y;

    if(y > x)
    {
      swap = x;
      x = y;
      y = swap;
    }
  }

  return x;
}

int nsd(int x, int y) __attribute__ ((pure));

int nsd(int x, int y)
{
  int swap;

  if(x <= 0 || y <= 0)
    return 0;

  if(x < y)
  {
    swap = x;
    x = y;
    y = swap;
  }

  while(x != y)
  {
    x = x - y;

    if(y > x)
    {
      swap = x;
      x = y;
      y = swap;
    }
  }

  return x;
}

int main(int argc, char **argv)
{
  return 0;
}
