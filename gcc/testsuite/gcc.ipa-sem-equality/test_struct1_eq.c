struct A
{
  int a;
  int b;
};

int foo(struct A *a)
{
  return 123;
}

int bar(struct A *b)
{
  return 123;
}

int main()
{
  return foo(0) + bar(0);
}
