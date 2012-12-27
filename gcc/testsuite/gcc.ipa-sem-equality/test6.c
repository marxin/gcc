#include <stdio.h>

struct container
{
  int x;
  int y;
};

static struct container max;
static int pole[3][3];

static struct container superpole[10][10];

void foo(void)
{
  printf("Sparta");
}

void f1(struct container *c)
{
//  struct container pes;
//  pes.x = 123;
//  struct container *pesp = &pes;
//  pesp->x = 5;
  pole[1][2] = 3;

  void (*f)(void) = &foo;

  superpole[4][3].x = 4;

  max.x = 3;
  void *x = &pole;

  int **a = (int**)pole;
  a[1][2] = 543;

  if(x != 0)
    pole[1][2] = 123;
}

void f2(struct container *c)
{
//  struct container pes;
//  pes.x = 123;
//  struct container *pesp = &pes;
//  pesp->x = 5;

  pole[1][2] = 3;

  void (*f)(void) = &foo;
  superpole[4][3].x = 4;
  max.x = 3;
  void *x = &pole;

  int **a = (int**)pole;
  a[1][2] = 543;

  if(x != 0)
    pole[1][2] = 123;
}

int main(int argc, char **argv)
{
  return 0;
}
