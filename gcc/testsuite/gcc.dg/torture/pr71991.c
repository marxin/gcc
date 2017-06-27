/* PR target/71991 */

/* { dg-do compile { target i?86-*-* x86_64-*-* } } */
/* { dg-options "-std=c99" } */

static inline int __attribute__ ((__always_inline__)) fn1 () { return 0; }
static inline int __attribute__ ((target("inline-all-stringops"))) fn2 () { return fn1 (); }

int main()
{
  return fn2();
}
