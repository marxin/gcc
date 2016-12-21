/* PR sanitizer/65280 */
/* { dg-do run } */
/* { dg-options "-fsanitize=bounds" } */

void
__attribute__((no_sanitize(("shift,shift-base,shift-exponent,integer-divide-by-zero,unreachable,vla-bound,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,alignment,nonnull-attribute,returns-nonnull-attribute,object-size,vptr"))))
foo (int n, int (*b)[n])
{
  (*b)[n] = 1;
}

int
__attribute__((no_sanitize(("shift,shift-base,shift-exponent,integer-divide-by-zero,unreachable,vla-bound,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,alignment,nonnull-attribute,returns-nonnull-attribute,object-size,vptr"))))
main ()
{
  int a[20];
  foo (3, (int (*)[3]) &a);
}

/* { dg-output "index 3 out of bounds for type 'int \\\[\\\*\\\]'" } */
