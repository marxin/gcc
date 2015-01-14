/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-sem-equality"  } */

__attribute__ ((noinline))
int zero()
{
  return 0;
}

__attribute__ ((noinline))
int nula()
{
  return 0;
}

__attribute__ ((noinline))
int foo()
{
  return zero();
}

__attribute__ ((noinline))
int bar()
{
  return nula();
}

int main()
{
  return foo() + bar();
}

/* { dg-final { scan-ipa-dump "Semantic equality hit:bar->foo" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Semantic equality hit:nula->zero" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Equal functions: 2" "sem-equality"  } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
