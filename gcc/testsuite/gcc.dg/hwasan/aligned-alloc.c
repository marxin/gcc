/* { dg-do compile } */
/*
   TODO
    This alignment isn't handled by the sanitizer interceptor alloc.
    At the moment this program fails at runtime in the libhwasan library.

    LLVM catches this problem at compile-time.
 */

int
main ()
{
  void *p = __builtin_aligned_alloc (17, 100);
  if ((unsigned long long)p & 0x10 == 0)
    return 0;
  __builtin_abort ();
}

