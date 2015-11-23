/* { dg-options "-O2 -fdump-tree-fnsplit" } */

void deallocate(void *);
void *a;

struct C {
  virtual void m_fn1();
};

struct D {
  C *m_fn2() {
    if (a)
      __builtin_abort();
  }
};
D getd();

struct vec_int {
  int _M_start;
  ~vec_int() {
    if (_M_start)
      deallocate(&_M_start);
  }
};
vec_int *b;

struct I {
  virtual void m_fn3();
};

void I::m_fn3() {
  if (a)
    getd().m_fn2()->m_fn1();
  b->~vec_int();
}

/* { dg-final { scan-tree-dump-times "Splitting function" 1 "fnsplit"} } */
