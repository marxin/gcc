int foo(void)
{
  return 123;
}

int bar(void)
{
  return 321;
}

int baz(void)
{
  return 321;
}

int main(int argc, char **argv)
{
  malloc(1);
  /*
  int (*f)(void);

  if(argc % 2)
    f = &foo;
  else
    f = &bar;

  return f();*/
  return foo() + bar() + baz();
}
