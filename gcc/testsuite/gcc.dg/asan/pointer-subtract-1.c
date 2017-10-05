// { dg-do run }
// { dg-shouldfail "asan" }
// { dg-set-target-env-var ASAN_OPTIONS "detect_invalid_pointer_pairs=1 halt_on_error=0" }
// { dg-options "-fsanitize=pointer-subtract -O0" }

int f(char *p, char *q)
{
  return p - q;
}

int f2(char *p)
{
  char *p2 = p + 20;
  __builtin_free(p);
  return p2 - p;
}

int
main ()
{
  char *p = (char *)__builtin_malloc(42);
  char *q = (char *)__builtin_malloc(42);

  int r = f(p, q) + f2(p);
  __builtin_free (q);

  return 1;
}

// { dg-output "ERROR: AddressSanitizer: invalid-pointer-pair.*(\n|\r\n|\r)*" }
// { dg-output "ERROR: AddressSanitizer: invalid-pointer-pair.*(\n|\r\n|\r)" }
