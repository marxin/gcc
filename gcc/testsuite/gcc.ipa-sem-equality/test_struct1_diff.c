struct A
{
  int a;
  int b;
};

struct B
{
  int x;
};

int foo(struct A *a)
{
  return 123;
}

int bar(struct B *b)
{
  return 123;
}

int main()
{
  return foo(0) + bar(0);
}
