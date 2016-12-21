/* { dg-do run } */
/* { dg-options "-fsanitize=nonnull-attribute,returns-nonnull-attribute" } */

int q, r;
void *a, *b, *c = (void *) &q, *d, *e, *f = (void *) &q, *g, *h;

__attribute__((returns_nonnull, nonnull (1, 3),no_sanitize(("shift,shift-base,shift-exponent,integer-divide-by-zero,unreachable,vla-bound,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,bounds-strict,alignment,object-size,vptr"))))
void *
foo (void *p, void *q, void *r)
{
  a = p;
  b = r;
  return q;
}

int
__attribute__((no_sanitize(("shift,shift-base,shift-exponent,integer-divide-by-zero,unreachable,vla-bound,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,bounds-strict,alignment,object-size,vptr"))))
bar (const void *a, const void *b)
{
  int c = *(const int *) a;
  int d = *(const int *) b;
  return c - d;
}

int
__attribute__((no_sanitize(("shift,shift-base,shift-exponent,integer-divide-by-zero,unreachable,vla-bound,return,null,signed-integer-overflow,bool,enum,float-divide-by-zero,float-cast-overflow,bounds,bounds-strict,alignment,object-size,vptr"))))
main ()
{
  asm volatile ("" : : : "memory");
  d = foo (c, b, c);
  e = foo (e, c, f);
  g = foo (c, f, g);
  __builtin_memset (d, '\0', q);
  return 0;
}

/* { dg-output "\.c:13:\[0-9]*:\[^\n\r]*null pointer returned from function declared to never return null\[^\n\r]*(\n|\r\n|\r)" } */
/* { dg-output "\[^\n\r]*\.c:31:\[0-9]*:\[^\n\r]*null pointer passed as argument 1, which is declared to never be null\[^\n\r]*(\n|\r\n|\r)" } */
/* { dg-output "\[^\n\r]*\.c:32:\[0-9]*:\[^\n\r]*null pointer passed as argument 3, which is declared to never be null\[^\n\r]*(\n|\r\n|\r)" } */
/* { dg-output "\[^\n\r]*\.c:33:\[0-9]*:\[^\n\r]*null pointer passed as argument 1, which is declared to never be null" } */
