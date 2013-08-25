/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-sem-equality"  } */

struct A
{
  int a, b, c, d;
};

struct B
{
  int x, y, z;
};

__attribute__ ((noinline))
int foo(struct A *a)
{
  a->c = 1;

  return 123;
}

__attribute__ ((noinline))
int bar(struct B *b)
{
  b->z = 1;

  return 123;
}

int main()
{
  return foo(0) + bar(0);
}

/* { dg-final { scan-ipa-dump-not "Semantic equality hit:" "sem-equality"  } } */
/* { dg-final { scan-ipa-dump "Equal functions: 0" "sem-equality"  } } */
/* { dg-final { cleanup-ipa-dump "sem-equality" } } */
