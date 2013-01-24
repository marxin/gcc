#include <complex.h>

double f1(void)
{
  double complex z1 = 1.0 + 3.0 * I;
  double complex z2 = 1.0 - 4.0 * I;

  return cimag(z1 + z2);
}

double f2(void)
{
  double complex z1 = 1.0 + 3.0 * I;
  double complex z2 = 1.0 - 4.0 * I;

  return cimag(z1 + z2);
}

int main()
{
  return 1;
}
