// { dg-do run }
// { dg-set-target-env-var ASAN_OPTIONS "detect_invalid_pointer_pairs=1:halt_on_error=1" }
// { dg-options "-fsanitize=address,pointer-subtract -O0" }

#include <unistd.h>
#include <pthread.h>

char *pointers[2];

void *
thread_main (void *n)
{
  char local;

  unsigned long id = (unsigned long) n;
  pointers[id] = &local;
  sleep (1);

  return 0;
}

int
main (int argc, char **argv)
{
  pthread_t threads[2];
  pthread_create (&threads[0], 0, thread_main, (void *) 0);
  pthread_create (&threads[1], 0, thread_main, (void *) 1);

  do
    {
    }
  while (pointers[0] == 0 || pointers[1] == 0);

  unsigned r = pointers[0] - pointers[1];

  pthread_join(threads[0], 0);
  pthread_join(threads[1], 0);

  return 0;
}
