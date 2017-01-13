/* { dg-do run } */
/* { dg-require-ifunc "" } */
/* { dg-require-effective-target lto } */
/* { dg-options "-flto" } */

__attribute__((target_clones("avx","arch=slm","arch=core-avx2","default")))
int
foo ()
{
  return -2;
}

int
bar ()
{
  return 2;
}

int
main ()
{
  int r = 0;
  r += bar ();
  r += foo ();
  r += bar ();
  r += foo ();
  r += bar ();
  return r - 2;
}
