/* { dg-do compile } */
/* { dg-options "-O2 -fdump-tree-optimized" } */

char *buffer1;

void
main_test (void)
{
  const char* const foo1 = "hello world";

  /* MEMCHR.  */
  if (__builtin_memchr (foo1, 'x', 11))
    __builtin_abort ();
  if (__builtin_memchr (foo1, 'x', 100))
    __builtin_abort ();
  if (__builtin_memchr (buffer1, 'x', 0) != 0)
    __builtin_abort ();
  if (__builtin_memchr (foo1, 'o', 11) != foo1 + 4)
    __builtin_abort ();
  if (__builtin_memchr (foo1, 'w', 2))
    __builtin_abort ();
  if (__builtin_memchr (foo1 + 5, 'o', 6) != foo1 + 7)
    __builtin_abort ();
  if (__builtin_memchr (foo1, 'd', 11) != foo1 + 10)
    __builtin_abort ();
  if (__builtin_memchr (foo1, 'd', 10))
    __builtin_abort ();
  if (__builtin_memchr (foo1, '\0', 11))
    __builtin_abort ();
  if (__builtin_memchr (foo1, '\0', 12) != foo1 + 11)
    __builtin_abort ();

  /* STRNCMP.  */
  if (__builtin_strncmp ("hello", "aaaaa", 0) != 0)
    __builtin_abort ();
  if (__builtin_strncmp ("aaaaa", "aaaaa", 100) != 0)
    __builtin_abort ();
  if (__builtin_strncmp ("aaaaa", "", 100) <= 0)
    __builtin_abort ();
  if (__builtin_strncmp ("", "aaaaa", 100) >= 0)
    __builtin_abort ();
  if (__builtin_strncmp ("ab", "ba", 1) >= 0)
    __builtin_abort ();

  /* STRNCASECMP.  */
  if (__builtin_strncasecmp ("hello", "aaaaa", 0) != 0)
    __builtin_abort ();
  if (__builtin_strncasecmp ("aaaaa", "aaaaa", 100) != 0)
    __builtin_abort ();
  if (__builtin_strncasecmp ("aaaaa", "", 100) <= 0)
    __builtin_abort ();
  if (__builtin_strncasecmp ("", "aaaaa", 100) >= 0)
    __builtin_abort ();
}

/* { dg-final { scan-tree-dump-not "__builtin_memchr" "optimized" } } */
/* { dg-final { scan-tree-dump-not "__builtin_strncmp" "optimized" } } */
/* { dg-final { scan-tree-dump-not "__builtin_strncasecmp" "optimized" } } */
/* { dg-final { scan-tree-dump-not "__builtin_abort" "optimized" } } */
