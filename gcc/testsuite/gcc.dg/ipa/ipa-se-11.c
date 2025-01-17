/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-sem-equality"  } */

__attribute__ ((noinline))
int fce(int a, int b)
{
  return a + b;
}

__attribute__ ((noinline))
int f0(int a)
{
  return fce(a, 5) + fce(a, 7);
}

__attribute__ ((noinline))
int f1(int a)
{
  return fce(a, 5) + fce(a, 7);
}

int main(int argc, char **argv)
{
  return f0(argc) * f1(argc);
}

/* { dg-final { scan-ipa-dump "Semantic equality hit:f1->f0" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Equal functions: 1" "sem-equality"  } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
