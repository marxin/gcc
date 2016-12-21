/* { dg-do run } */
/* { dg-options "-fsanitize=unreachable" } */
/* { dg-shouldfail "ubsan" } */

int
__attribute__((no_sanitize(("shift,shift-base,shift-exponent,integer-divide-by-zero,vla-bound,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,bounds-strict,alignment,nonnull-attribute,returns-nonnull-attribute,object-size,vptr"))))
main (void)
{
  __builtin_unreachable ();
}
 /* { dg-output "execution reached a __builtin_unreachable\\(\\) call" } */
