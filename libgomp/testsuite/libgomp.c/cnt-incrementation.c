#include <omp.h>
#include <stdlib.h>

#define SIZE 100000

int
main ()
{
  unsigned counter = 0;

  int j;
#pragma omp target map(tofrom : counter)
  {
    omp_set_num_threads (1);

    for (j = 0; j < SIZE; j++)
      counter++;

    for (j = 0; j < SIZE; j++)
      counter++;

    for (j = 0; j < SIZE; j++)
      counter++;

    for (j = 0; j < SIZE; j++)
      counter++;

    for (j = 0; j < SIZE; j++)
      counter++;
  }

  unsigned expected = 5 * SIZE;
  if (counter != expected)
    abort ();

  return 0;
}
