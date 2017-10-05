// { dg-do run }
// { dg-set-target-env-var ASAN_OPTIONS "detect_invalid_pointer_pairs=1 halt_on_error=0" }
// { dg-options "-fsanitize=pointer-compare -O0" }

int f(char *p)
{
  char *p2 = p + 20;
  return p > p2;
}

int
main ()
{
  char *p = (char *)__builtin_malloc(42);

  int r = f(p);
  __builtin_free (p);
  return 0;
}
