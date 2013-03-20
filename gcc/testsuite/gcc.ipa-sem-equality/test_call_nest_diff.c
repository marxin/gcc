int foo(int a)
{
  return a * a;
}

int bar(int b)
{
  return b;
}

void caller(int x)
{
  return;
}

int main(int argc, char **argv)
{
  caller(foo(argc));
  caller(bar(argc));

  return 123;
}
