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

/*
void x1(int x)
{
  pole2[x + 1] = 13;
}

void x2(int a)
{
  pole2[a + 1] = 13;
}
*/

void f1(struct container *c)
{
  struct container pes;
  void *x = &pes + 123;

  /*
  c->x = 5;
  pes.x = 123;
  struct container *pesp = &pes;
  pesp->x = 5;

  /*
  pole[1][2] = 3;

  void (*f)(void) = &foo;

  superpole[4][3].x = 4;

  max.x = 3;
  void *x = &pole;

  int **a = (int**)pole;
  a[1][2] = 543;

  if(x != 0)
    pole[1][2] = 123;
  */
}

void f2(struct container *c)
{
  struct container pes;
  void *x = &pes + 123;

/*
  c->x = 5;
  struct container pes;
  pes.x = 123;
  struct container *pesp = &pes;
  pesp->x = 5;

/*
  pole[1][2] = 3;

  void (*f)(void) = &foo;
  superpole[4][3].x = 4;
  max.x = 3;
  void *x = &pole;

  int **a = (int**)pole;
  a[1][2] = 543;

  if(x != 0)
    pole[1][2] = 123;
*/
}

int main(int argc, char **argv)
{
  return 0;
}
