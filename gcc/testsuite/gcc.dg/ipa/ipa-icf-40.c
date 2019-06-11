/* { dg-do compile } */
/* { dg-options "-O2 -fdump-ipa-icf"  } */

struct A { int i; char a1[10]; };
struct B { int i; char a3[30]; };
struct C { int i; char ax[]; };

static int
__attribute__((noinline))
test_array_1 (int i, struct A *a)
{
  return __builtin_printf ("%-s\n", a->a1);
}

static int
__attribute__((noinline))
test_array_3 (int i, struct B *b)
{
  return __builtin_printf ("%-s\n", b->a3);
}

struct A a = { 0, "foo" };
struct B b = { 0, "bar" };

int main()
{
  test_array_1 (0, &a);
  test_array_3 (0, &b);
  return 0;
}

/* { dg-final { scan-ipa-dump "Equal symbols: 1" "icf"  } } */
