/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-sem-equality"  } */

#include <stdio.h>

__attribute__ ((noinline))
int foo()
{
  printf ("Hello world.\n");
  return 0;
}

__attribute__ ((noinline))
int bar()
{
  printf ("Hello world.\n");
  return 0;
}

int main()
{
  return foo() + bar();
}

/* { dg-final { scan-ipa-dump "Semantic equality hit:bar->foo" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Equal functions: 1" "sem-equality"  } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
