int zero()
{
  return 0;
}

int nula()
{
  return 0;
}

int foo()
{
  return zero();
}

int bar()
{
  return nula();
}

int main()
{
  return foo() + bar();
}
