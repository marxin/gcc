/* PR sanitizer/79558 */
/* { dg-do compile } */
/* { dg-options "-fsanitize=bounds" } */

void
__attribute__((no_sanitize(("shift,shift-base,shift-exponent,integer-divide-by-zero,unreachable,vla-bound,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,alignment,nonnull-attribute,returns-nonnull-attribute,object-size,vptr"))))
fn1 (int n)
{
  int i, j;
  int x[2][0];
  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++)
      x[i][j] = 5;
}
