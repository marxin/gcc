/* { dg-do run } */
/* { dg-options "-O2 -fdump-tree-optimized" } */

char *buffer1;
char *buffer2;

#define SIZE 1000

int
main (void)
{
  const char* const foo1 = "hello world";

  buffer1 = __builtin_malloc (SIZE);
  __builtin_strcpy (buffer1, foo1);
  buffer2 = __builtin_malloc (SIZE);
  __builtin_strcpy (buffer2, foo1);

  /* MEMCHR.  */
  if (__builtin_memchr (foo1, 'x', 11))
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

  /* STRCMP.  */
  if (__builtin_strcmp ("hello", "aaaaa") <= 0)
    __builtin_abort ();
  if (__builtin_strcmp ("aaaaa", "aaaaa") != 0)
    __builtin_abort ();
  if (__builtin_strcmp ("aaaaa", "") <= 0)
    __builtin_abort ();
  if (__builtin_strcmp ("", "aaaaa") >= 0)
    __builtin_abort ();
  if (__builtin_strcmp ("ab", "ba") >= 0)
    __builtin_abort ();

  /* STRCASECMP.  */
  if (__builtin_strcasecmp ("aaaaa", "aaaaa") != 0)
    __builtin_abort ();
  if (__builtin_strcasecmp ("aaaaa", "") <= 0)
    __builtin_abort ();
  if (__builtin_strcasecmp ("", "aaaaa") >= 0)
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
  if (__builtin_strncmp ("aab", "aac", 2) != 0)
    __builtin_abort ();
  if (__builtin_strncmp (buffer1, buffer2, 1) != 0)
    __builtin_abort (); /* not folded away */

  /* STRNCASECMP.  */
  if (__builtin_strncasecmp ("hello", "aaaaa", 0) != 0)
    __builtin_abort ();
  if (__builtin_strncasecmp ("aaaaa", "aaaaa", 100) != 0)
    __builtin_abort ();
  if (__builtin_strncasecmp ("aaaaa", "", 100) <= 0)
    __builtin_abort ();
  if (__builtin_strncasecmp ("", "aaaaa", 100) >= 0)
    __builtin_abort ();
  if (__builtin_strncasecmp ("aab", "aac", 2) != 0)
    __builtin_abort ();
  if (__builtin_strncasecmp ("ab", "ba", 1) >= 0) /* not folded away */
    __builtin_abort (); /* not folded away */
  if (__builtin_strncasecmp (buffer1, buffer2, 1) != 0) /* not folded away */
    __builtin_abort (); /* not folded away */
  if (__builtin_strncasecmp (buffer1, buffer2, 100) != 0) /* not folded away */
    __builtin_abort (); /* not folded away */

  return 0;
}

/* { dg-final { scan-tree-dump-not "__builtin_strcmp" "optimized" } } */
/* { dg-final { scan-tree-dump-not "__builtin_strcasecmp" "optimized" } } */
/* { dg-final { scan-tree-dump-not "__builtin_strncmp" "optimized" } } */
/* { dg-final { scan-tree-dump-not "__builtin_memchr" "optimized" } } */
/* { dg-final { scan-tree-dump-times "__builtin_strncasecmp" 3 "optimized" } } */
