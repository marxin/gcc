/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-sem-equality"  } */

#include <xmmintrin.h>

__attribute__ ((noinline))
void foo()
{
  float x = 1.2345f;
  __m128 v =_mm_load1_ps(&x);
}

__attribute__ ((noinline))
void bar()
{
  float x = 1.2345f;
  __m128 v =_mm_load1_ps(&x);
}

int main()
{
  return 2;
}

/* { dg-final { scan-ipa-dump "Semantic equality hit:bar->foo" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Equal functions: 1" "sem-equality"  } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
