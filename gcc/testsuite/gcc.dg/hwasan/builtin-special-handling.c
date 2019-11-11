/* { dg-do compile } */
/* { dg-options "-fdump-tree-hwasan" } */
/* { dg-skip-if "" { *-*-* }  { "-O0" } { "" } } */

typedef __SIZE_TYPE__ size_t;
/* Functions to observe that HWASAN instruments memory builtins in the expected
   manner.  */
void * __attribute__((noinline))
memset_builtin (void *dest, int value, size_t len)
{
  return __builtin_memset (dest, value, len);
}

/* HWASAN avoids strlen because it doesn't know the size of the memory access
   until *after* the function call.  */
size_t __attribute__ ((noinline))
strlen_builtin (char *element)
{
  return __builtin_strlen (element);
}

/* First test ensures that the HWASAN_CHECK was emitted before the
   memset.  Second test ensures there was only HWASAN_CHECK (which demonstrates
   that strlen was not instrumented).  */
/* { dg-final { scan-tree-dump-times "HWASAN_CHECK.*memset" 1 "hwasan1" } } */
/* { dg-final { scan-tree-dump-times "HWASAN_CHECK" 1 "hwasan1" } } */
