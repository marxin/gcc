int a(int v);
int b(int v);
int c(int v);

int a(int v)
{
  if (v < 111)
    return;

  return b(v + 1);
}

int b(int v)
{
  if (v < 10)
    return;

  return c(v + 1);
}

int c(int v)
{
  if (v < 10)
    return;

  return a(v + 1);
}

int x(int v);
int y(int v);
int z(int v);

int x(int v)
{
  if (v < 10)
    return;

  return y(v + 1);
}

int y(int v)
{
  if (v < 10)
    return;

  return z(v + 1);
}

int z(int v)
{
  if (v < 10)
    return;

  return x(v + 1);
}
int main()
{
  return x(1) + a(1);
}
