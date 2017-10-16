/* PR sanitizer/82545.  */
/* { dg-do compile } */

extern void c(int);
extern void d(void);

void a(void) {
  {
    int b;
    &b;
    __builtin_setjmp(0);
    c(b);
  }
  d();
}
