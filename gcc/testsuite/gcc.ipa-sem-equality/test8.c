#include <complex.h>

#if (__SIZEOF_INT__ == __SIZEOF_FLOAT__)
typedef int intflt;
#elif (__SIZEOF_LONG__ == __SIZEOF_FLOAT__)
typedef long intflt;
#else
#error Add target support here for type that will union float size
#endif


static double test;

struct struktura
{
  union
  {
    long i;
    float f;
  } u;
};

struct struktura sss;

struct X {
  int i;
  union {
    intflt j;
    intflt k;
    float f;
  } u;
};

intflt foo(intflt j)
{
  struct X a;

  a.u.j = j;
  a.u.f = a.u.f;
  a.u.f = a.u.f;
  a.u.j = a.u.j;
  a.u.f = a.u.f;
  return a.u.k;
}

intflt foo2(intflt j)
{
  struct X a;

  a.u.j = j;
  a.u.f = a.u.f;
  a.u.f = a.u.f;
  a.u.j = a.u.j;
  a.u.f = a.u.f;
  return a.u.k;
}
float f1(long i)
{
  float x = (float)i;
  sss.u.i = i;
  return sss.u.f;
}

float f2(long i)
{
  float x = (float)i;

  sss.u.i = i;
  return sss.u.f;
}

int main()
{
  return 1;
}
