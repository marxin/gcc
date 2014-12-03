#include <stdio.h>

void __attribute__((hsa))
square (int *a)
{
  *a += 99;
}

void __attribute__((hsakernel))
foo (int *a, int *b, ...)
{
  int i = a[__builtin_omp_get_thread_num ()];
  int *x = &i;
  square (x);
  b[__builtin_omp_get_thread_num ()] = i;
  a[__builtin_omp_get_thread_num ()] = __builtin_omp_get_thread_num ();
}

#define N 16
int a[N];
int b[N];


int
main (int argc, char **argv)
{
  int i;
  for (i = 4 ; i <= N; i += 4)
    {
      int j;
      for (j = 0; j < N; j++)
	a[j] = j;
      __builtin_memset (b, 0, sizeof(int) * N);
      foo (a, b, i, 4);
      for (j = 0; j < N; j++)
	fprintf (stdout, "%.3i ", a[j]);
      fprintf (stdout, "\n");
      for (j = 0; j < N; j++)
	fprintf (stdout, "%.3i ", b[j]);
      fprintf (stdout, "\n\n");
    }
  return 0;
}
