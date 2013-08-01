/* { dg-do compile } */
/* { dg-options "-O2 -fipa-sem-equality -fdump-ipa-sem-equality"  } */

__attribute__ ((noinline))
int foo()
{
  return 1;
}

__attribute__ ((noinline))
int bar()
{
  return 1;
}

int main()
{
  return foo() + bar();
}

/* { dg-final { scan-ipa-dump "Semantic equality hit" "sem-equality" } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
