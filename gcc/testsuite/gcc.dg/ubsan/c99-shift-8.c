/* { dg-do run } */
/* { dg-options "-fsanitize=shift-exponent -w -std=c99" } */

int
__attribute__((no_sanitize(("shift-base,integer-divide-by-zero,unreachable,vla-bound,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,bounds-strict,alignment,nonnull-attribute,returns-nonnull-attribute,object-size,vptr"))))
main (void)
{
  int b = 43;
  volatile int c = 129;
  b << c;
}
/* { dg-output "shift exponent 129 is too large for" } */
