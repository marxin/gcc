/* dg-options "-O0" */

#include <limits.h>

#define T unsigned int
#define BITSIZE CHAR_BIT * sizeof (T)

#define C1 123u

T
rotate (T value, T shift)
{
  T r = (value << shift) | (value >> (BITSIZE - shift));
  return (r >> shift) | (r << (BITSIZE - shift));
}

int
main (int argc)
{
  T v1, v2, v3, v4, v5;

#pragma omp target map(to: v1, v2, v3, v4, v5)
  {
    v1 = rotate (C1, 10);
    v2 = rotate (C1, 2);
    v3 = rotate (C1, 5);
    v4 = rotate (C1, 16);
    v5 = rotate (C1, 32);
  }

  __builtin_assert (v1 == C1);
  __builtin_assert (v2 == C1);
  __builtin_assert (v3 == C1);
  __builtin_assert (v4 == C1);
  __builtin_assert (v5 == C1);

  return 0;
}
