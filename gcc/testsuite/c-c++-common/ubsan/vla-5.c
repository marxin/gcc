/* { dg-do run } */
/* { dg-options "-fsanitize=vla-bound -fno-sanitize-recover=vla-bound" } */

int
__attribute__((no_sanitize(("shift,shift-base,shift-exponent,integer-divide-by-zero,unreachable,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,bounds-strict,alignment,nonnull-attribute,returns-nonnull-attribute,object-size,vptr"))))
main (void)
{
  int x = 1;
  /* Check that the size of an array is evaluated only once.  */
  int a[++x];
  if (x != 2)
    __builtin_abort ();
  return 0;
}
