/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-sem-equality"  } */

#include <math.h>

__attribute__ ((noinline))
float foo()
{
  return sin(12.4f);
}

__attribute__ ((noinline))
float bar()
{
  return sin(12.4f);
}

int main()
{
  foo();
  bar();

  return 0;
}

/* { dg-final { scan-ipa-dump "Semantic equality hit:bar->foo" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Equal functions: 1" "sem-equality"  } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
