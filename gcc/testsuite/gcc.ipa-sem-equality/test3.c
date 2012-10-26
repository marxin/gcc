struct box
{
  int x, y, z;
};

int ferda(int x, int y) __attribute__ ((pure));
int funkce(int a, int b) __attribute__ ((pure));

int ferda(int x, int y)
{
  if (x < y)
  {
    return x;
  }
  else
    return y;
}

int funkce(int a, int b)
{
  if(a < b)
    return a;
  else
    return b;
}

int fce(int a, int b)
{
  if(a < b)
    goto pepa;

  a = b;

  pepa:

  return a * b;
}

struct box fce2(int x, int y)
{
  struct box b;

  return b;
//  return x * y;
}

int main(int argc, char **argv)
{
  return fce(argc, argc) + fce2(argc, argc).x;
}
