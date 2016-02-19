/* { dg-require-effective-target offload_hsa } */

#include <stdlib.h>

#define size 10
int i, j, k;

int
main ()
{
  char *s = malloc (size + 1);

#pragma omp target teams
  {
#pragma omp distribute parallel for default(none) private(i) shared(s)
    for (i = 0; i < size; ++i)
      {
	char *buffer = __builtin_alloca (10);
	buffer[5] = 97 + i;
	s[i] = buffer[5];
      }
  }

  for (i = 0; i < size; ++i)
    if (s[i] != 97 + i)
      abort ();

  return 0;
}
