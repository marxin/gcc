/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-sem-equality"  } */

__attribute__ ((noinline))
int foo(int x)
{
  int c = x;

  if (x > 10)
    c += 2;
  else
    c -= 3;

  return c;
}

__attribute__ ((noinline))
int bar(int y)
{
  int d = y;

  if (y > 10)
    d += 2;
  else
    d -= 3;

  return d;
}

int main()
{
  return 0;
}

/* { dg-final { scan-ipa-dump "Semantic equality hit:bar->foo" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Equal functions: 1" "sem-equality"  } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
