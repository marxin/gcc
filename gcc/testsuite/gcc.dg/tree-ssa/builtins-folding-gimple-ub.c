/* { dg-do compile } */
/* { dg-options "-O1 -fdump-tree-optimized" } */

char *buffer1;
char *buffer2;

const char global1[] = {'a', 'b', 'c', 'd'};
const char global2[] = {'a', 'b', '\0', 'd', '\0'};
const char global3[] = {'a', 'b', [3] = 'x', 'c', '\0'};
const char global4[] = {'a', 'b', 'c', 'd', [5] = '\0'};
char global5[] = {'a', 'b', 'c', 'd', '\0'};

#define SIZE 1000

int
main (void)
{
  char null = '\0';
  const char* const foo1 = "hello world";

  /* MEMCHR.  */
  if (__builtin_memchr ("", 'x', 1000)) /* Not folded away.  */
    __builtin_abort ();
  if (__builtin_memchr (foo1, 'x', 1000)) /* Not folded away.  */
    __builtin_abort ();

  if (__builtin_memchr (global1, null, 1) == foo1) /* Not folded away.  */
    __builtin_abort ();
  if (__builtin_memchr (global2, null, 1) == foo1) /* Not folded away.  */
    __builtin_abort ();
  if (__builtin_memchr (global3, null, 1) == foo1) /* Not folded away.  */
    __builtin_abort ();
  if (__builtin_memchr (global4, null, 1) == foo1) /* Not folded away.  */
    __builtin_abort ();
  if (__builtin_memchr (global5, null, 1) == foo1) /* Not folded away.  */
    __builtin_abort ();

  /* STRNCMP.  */
  if (strncmp ("a", "b", -1)) /* { dg-warning "implicit declaration of function" } */
    __builtin_abort ();

  return 0;
}

/* { dg-final { scan-tree-dump-times "__builtin_memchr" 7 "optimized" } } */
