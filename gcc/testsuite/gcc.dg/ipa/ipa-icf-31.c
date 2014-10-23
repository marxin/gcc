/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-icf"  } */

__attribute__ ((noinline))
__attribute__ ((target("arch=atom")))
int foo(int a)
{
  return a * a;
}

__attribute__ ((noinline))
__attribute__ ((target("sse4.2")))
int bar(int b)
{
  return b * b;
}

int main()
{
  return 1;
}

/* { dg-final { scan-ipa-dump "Equal symbols: 0" "icf"  } } */
/* { dg-final { cleanup-ipa-dump "icf" } } */
