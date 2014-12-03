#include <stdio.h>

int __attribute__((hsa))
mul (int a, int b)
{
  return a * b;
}

void __attribute__((hsakernel))
foo (int *a, int *b, ...)
{
  int i = a[__builtin_omp_get_thread_num ()];
  b[__builtin_omp_get_thread_num ()] = mul (i, i);
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
