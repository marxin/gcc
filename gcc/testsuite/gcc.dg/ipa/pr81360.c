/* { dg-options "-O2"  } */

void e(int *, const char *);

void b(void *c, int d) {
  if (c)
    e(0, __PRETTY_FUNCTION__);
}

void __attribute__((optimize(0))) h() {
  long i = 123;
  b((void *)i, sizeof(int));
}

