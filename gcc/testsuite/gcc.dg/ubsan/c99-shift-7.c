/* { dg-do run } */
/* { dg-options "-fsanitize=shift -w -std=c99" } */

int
__attribute__((no_sanitize(("shift-exponent,integer-divide-by-zero,unreachable,vla-bound,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,bounds-strict,alignment,nonnull-attribute,returns-nonnull-attribute,object-size,vptr"))))
main (void)
{
  int a = -42;
  a << 1;
}
/* { dg-output "left shift of negative value -42" } */
