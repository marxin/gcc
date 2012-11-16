int f1(void)
{
  int x = 123;

  void *ptr = &&a;

  if(x == 1)
    goto *ptr;

  ++x;

  a:
    return 2;
}

int f2(void)
{
  int x = 123;

  void *ptr = &&a;

  if(x == 1)
    goto *ptr;

  ++x;

  a:
    return 2;
}

int main(int argc, char **argv)
{
  return 0;
}
