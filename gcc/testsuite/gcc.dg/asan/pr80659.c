/* PR sanitizer/80659 */
/* { dg-do compile } */

void foo(int a)
{
  switch (a) {
    (int[3]){}; /* { dg-warning "statement will never be executed" } */
    int h;
  }
}
