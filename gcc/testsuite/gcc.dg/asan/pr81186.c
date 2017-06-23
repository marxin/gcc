/* PR sanitizer/81186 */
/* { dg-do run } */

int
main ()
{
  __label__ l;
  void f () { goto l; }

  f ();
l:
  return 0;
}
