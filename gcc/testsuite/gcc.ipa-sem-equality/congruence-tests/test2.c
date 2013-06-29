int x()
{
  return 1;
}

int y()
{
  return 1;
}

int c()
{
  return y();
}

int a()
{
  return x();
}

int b()
{
  return 3;
}

int foo()
{
  return a();
}

int bar()
{
  return c();
}

int baz()
{
  return b();
}

int main()
{
  return foo() + bar() + baz();
}
