/* { dg-options "-O -fdump-tree-reassoc-folding-details" } */

#define TEST(N, OP1, V1, OP2, V2, EXP) \
  static void test_ ## N ## _worker (int argc, int v1, int v2) \
  { \
    if (((argc OP1 v1) && (argc OP2 v2)) != EXP) \
      __builtin_abort (); \
    \
  } \
  static void __attribute__((noinline)) test_ ## N (int argc) \
  { \
     test_ ## N ## _worker (argc, V1, V2); \
  }

TEST (1, <, 11, ==, 22, 0);
TEST (2, <, 11, ==, 2, 1);
TEST (3, >, 11, ==, 22, 1);
TEST (4, >, 11, ==, 2, 0);

int main(int argc)
{
  test_1 (argc);
  test_2 (argc);
  test_3 (argc);
  test_4 (argc);
}

/* { dg-final { scan-tree-dump-times "Applying pattern match.pd" 4 "reassoc1" } } */
