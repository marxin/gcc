int foo(void)
{
  char *b;

  try
  {
    b = new char[123];
  }
  catch(int a)
  {
    return 1;
  }

  return 123;
}

int bar(void)
{
  char *b;

  try
  {
    b = new char[123];
  }
  catch(int a)
  {
    return 1;
  }

  return 123;
}

int main()
{
  return foo() + bar();
}
