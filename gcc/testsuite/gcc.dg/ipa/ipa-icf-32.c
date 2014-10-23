/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-icf-details"  } */

#pragma GCC optimize("-O2")

__attribute__ ((noinline))
int foo(int a)
{
  return a * a;
}

#pragma GCC optimize("-O3")

__attribute__ ((noinline))
int bar(int b)
{
  return b * b;
}

#pragma GCC reset_options

int main()
{
  return 1;
}

/* { dg-final { scan-ipa-dump "Equal symbols: 0" "icf"  } } */
/* { dg-final { scan-ipa-dump "different function specific options are used" "icf"  } } */
/* { dg-final { cleanup-ipa-dump "icf" } } */
