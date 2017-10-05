// { dg-do run }
// { dg-set-target-env-var ASAN_OPTIONS "detect_invalid_pointer_pairs=1 halt_on_error=1" }
// { dg-options "-fsanitize=pointer-compare -O0" }

int foo(char *p)
{
  char *p2 = p + 20;
  return p > p2;
}

int bar(char *p, char *q)
{
  return p <= q;
}

char global[10000] = {};
char small_global[7] = {};

int
main ()
{
  /* Heap allocated memory.  */
  char *p = (char *)__builtin_malloc(42);
  int r = foo(p);
  __builtin_free (p);

  /* Global variable.  */
  bar(&global[0], &global[1]);
  bar(&global[1], &global[2]);
  bar(&global[2], &global[1]);
  bar(&global[0], &global[100]);
  bar(&global[1000], &global[9000]);
  bar(&global[500], &global[10]);
  bar(&small_global[0], &small_global[1]);
  bar(&small_global[0], &small_global[7]);
  bar(&small_global[7], &small_global[1]);
  bar(&small_global[6], &small_global[7]);
  bar(&small_global[7], &small_global[7]);

  /* Stack variable.  */
  char stack[10000];
  bar(&stack[0], &stack[100]);
  bar(&stack[1000], &stack[9000]);
  bar(&stack[500], &stack[10]);

  return 0;
}
