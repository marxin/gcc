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

// problem: missing pure
int nsd_not_pure(int x, int y)
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

/* OK! */
int nsd2(int x, int y) __attribute__ ((pure));

int nsd2(int x, int y)
{
  int pes;

  if(x <= 0 || y <= 0)
    return 0;

  if(x < y)
  {
    pes = x;
    x = y;
    y = pes;
  }

  while(x != y)
  {
    x = x - y;

    if(y > x)
    {
      pes = x;
      x = y;
      y = pes;
    }
  }

  return x;
}

int nsd_reorder(int x, int y) __attribute__ ((pure));

int nsd_reorder(int x, int y)
{
  int swap;

  /* problem: switched condition */
  if(y <= 0 || x <= 0)
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

int nsd_different_result(int x, int y) __attribute__ ((pure));

int nsd_different_result(int x, int y)
{
  int pes;

  if(x <= 0 || y <= 0)
    return 1;

  if(x < 10)
    y = 12;
  else if(x == 44)
    y = 124;
  else
    y = 1111;

  if(x < y)
  {
    pes = x;
    x = y;
    y = pes;
  }

  while(x != y)
  {
    x = x - y;

    if(y > x)
    {
      pes = x;
      x = y;
      y = pes;
    }
  }

  return x;
}

int nsd_different_result2(int x, int y) __attribute__ ((pure));

int nsd_different_result2(int x, int y)
{
  int pes;

  if(x <= 0 || y <= 0)
    return 1;

  if(x < 10)
    y = 12;
  else if(x == 44)
    y = 124;
  else
    y = 1111;

  if(x < y)
  {
    pes = x;
    x = y;
    y = pes;
  }

  while(x != y)
  {
    x = x - y;

    if(y > x)
    {
      pes = x;
      x = y;
      y = pes;
    }
  }

  return x;
}

int main(int argc, char **argv)
{
  if(argc < 3)
    return 1;

  int a = atoi(argv[1]);
  int b = atoi(argv[2]);

  printf("Test1: %d, %d, gdc: %d\n", a, b, gcd(a, b));
  printf("Test2: %d, %d, gdc: %d\n", a, b, nsd(a, b));

}
