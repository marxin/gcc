/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-sem-equality"  } */

struct A
{
  int a;
  int b;
};

__attribute__ ((noinline))
int foo(struct A *a)
{
  return 123;
}

__attribute__ ((noinline))
int bar(struct A *b)
{
  return 123;
}

int main()
{
  return foo(0) + bar(0);
}

/* { dg-final { scan-ipa-dump "Semantic equality hit:bar->foo" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Equal functions: 1" "sem-equality"  } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
