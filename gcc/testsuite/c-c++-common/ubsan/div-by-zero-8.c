/* { dg-do compile } */
/* { dg-options "-fsanitize=integer-divide-by-zero" } */

void
__attribute__((no_sanitize(("shift,shift-base,shift-exponent,unreachable,vla-bound,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,bounds-strict,alignment,nonnull-attribute,returns-nonnull-attribute,object-size,vptr"))))
foo (void)
{
  int A[-2 / -1] = {};
}
