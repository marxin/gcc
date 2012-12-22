struct container
{
  int x;
  int y;
};

static struct container max;
static int pole[3][3];

void f1(struct container *c)
{
  void *x = &pole;

  if(x != 0)
    pole[1][2] = 123;
}

void f2(struct container *c)
{
  void *x = &pole;

  if(x != 0)
    pole[1][2] = 123;
}

int main(int argc, char **argv)
{
  return 0;
}
