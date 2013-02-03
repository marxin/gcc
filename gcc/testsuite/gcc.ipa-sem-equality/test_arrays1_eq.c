#include <stdio.h>

struct container
{
  int x;
  int y;
};

static struct container max;
static int pole[3][3];
static int pole2[123];

static struct container superpole[10][10];

void foo(void)
{
  printf("Sparta");
}

int order(int x, int y)
{
   return x < y ? 2 : 4; 
}

int order2(int y, int x)
{
   return x < y ? 2 : 4; 
}

void x1(int x)
{
  int i;
  for(i = 0; i < 20; ++i)
    pole2[i] = i;

  pole2[2] = 13;
}

void x2(int a)
{
  int i;
  for(i = 0; i < 20; ++i)
    pole2[i] = i;

  pole2[2] = 13;
}

int main(int argc, char **argv)
{
  return 0;
}
