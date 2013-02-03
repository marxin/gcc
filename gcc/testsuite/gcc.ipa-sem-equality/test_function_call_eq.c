int fce(int a, int b)
{
  return a + b;
}

int f0(int a)
{
  return fce(a, 5) + fce(a, 7);
}

int f1(int a)
{
  return fce(a, 5) + fce(a, 7);
}

int main(int argc, char **argv)
{
  return f0(argc) * f1(argc);
}
