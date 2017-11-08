// { dg-do run }
// { dg-shouldfail "asan" }
// { dg-set-target-env-var ASAN_OPTIONS "detect_invalid_pointer_pairs=1:halt_on_error=1" }
// { dg-options "-fsanitize=address,pointer-subtract -O0" }

#include <unistd.h>
#include <pthread.h>

char *pointer;

void *
thread_main (void *n)
{
  char local;
  pointer = &local;
  sleep (1);

  return 0;
}

int
main (int argc, char **argv)
{
  pthread_t thread;
  pthread_create (&thread, 0, thread_main, (void *) 0);

  do
    {
    }
  while (pointer == 0);

  char local;
  char *parent_pointer = &local;

  // { dg-output "ERROR: AddressSanitizer: invalid-pointer-pair" }
  unsigned r = parent_pointer - pointer;
  pthread_join(thread, 0);

  return 0;
}
